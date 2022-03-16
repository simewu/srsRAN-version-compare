/*
 * Copyright 2013-2019 Software Radio Systems Limited
 *
 * This file is part of srsLTE.
 *
 * srsLTE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsLTE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#include "srslte/common/log.h"
#include "srslte/common/threads.h"
#include "srslte/srslte.h"

#include "srsenb/hdr/phy/cc_worker.h"

#define Error(fmt, ...)                                                                                                \
  if (SRSLTE_DEBUG_ENABLED)                                                                                            \
  log_h->error(fmt, ##__VA_ARGS__)
#define Warning(fmt, ...)                                                                                              \
  if (SRSLTE_DEBUG_ENABLED)                                                                                            \
  log_h->warning(fmt, ##__VA_ARGS__)
#define Info(fmt, ...)                                                                                                 \
  if (SRSLTE_DEBUG_ENABLED)                                                                                            \
  log_h->info(fmt, ##__VA_ARGS__)
#define Debug(fmt, ...)                                                                                                \
  if (SRSLTE_DEBUG_ENABLED)                                                                                            \
  log_h->debug(fmt, ##__VA_ARGS__)

using namespace std;

// Enable this to log SI
//#define LOG_THIS(a) 1

// Enable this one to skip SI-RNTI
#define LOG_THIS(rnti) (rnti != 0xFFFF)

/*********************************************/

using namespace asn1::rrc;

//#define DEBUG_WRITE_FILE

namespace srsenb {

cc_worker::cc_worker()
{
  reset();
}

cc_worker::~cc_worker()
{
  srslte_softbuffer_tx_free(&temp_mbsfn_softbuffer);
  srslte_enb_dl_free(&enb_dl);
  srslte_enb_ul_free(&enb_ul);

  for (int p = 0; p < SRSLTE_MAX_PORTS; p++) {
    if (signal_buffer_rx[p]) {
      free(signal_buffer_rx[p]);
    }
    if (signal_buffer_tx[p]) {
      free(signal_buffer_tx[p]);
    }
  }

  // Delete all users
  for (auto& it : ue_db) {
    delete it.second;
  }
}

#ifdef DEBUG_WRITE_FILE
FILE* f;
#endif

void cc_worker::init(phy_common* phy_, srslte::log* log_h_, uint32_t cc_idx_)
{
  phy    = phy_;
  log_h  = log_h_;
  cc_idx = cc_idx_;

  // Init cell here
  for (int p = 0; p < SRSLTE_MAX_PORTS; p++) {
    signal_buffer_rx[p] = (cf_t*)srslte_vec_malloc(2 * SRSLTE_SF_LEN_PRB(phy->cell.nof_prb) * sizeof(cf_t));
    if (!signal_buffer_rx[p]) {
      ERROR("Error allocating memory\n");
      return;
    }
    bzero(signal_buffer_rx[p], 2 * SRSLTE_SF_LEN_PRB(phy->cell.nof_prb) * sizeof(cf_t));
    signal_buffer_tx[p] = (cf_t*)srslte_vec_malloc(2 * SRSLTE_SF_LEN_PRB(phy->cell.nof_prb) * sizeof(cf_t));
    if (!signal_buffer_tx[p]) {
      ERROR("Error allocating memory\n");
      return;
    }
    bzero(signal_buffer_tx[p], 2 * SRSLTE_SF_LEN_PRB(phy->cell.nof_prb) * sizeof(cf_t));
  }
  if (srslte_enb_dl_init(&enb_dl, signal_buffer_tx, phy->cell.nof_prb)) {
    ERROR("Error initiating ENB DL\n");
    return;
  }
  if (srslte_enb_dl_set_cell(&enb_dl, phy->cell)) {
    ERROR("Error initiating ENB DL\n");
    return;
  }
  if (srslte_enb_ul_init(&enb_ul, signal_buffer_rx[0], phy->cell.nof_prb)) {
    ERROR("Error initiating ENB UL\n");
    return;
  }
  if (srslte_enb_ul_set_cell(&enb_ul, phy->cell, &phy->ul_cfg_com.dmrs)) {
    ERROR("Error initiating ENB UL\n");
    return;
  }

  /* Setup SI-RNTI in PHY */
  add_rnti(SRSLTE_SIRNTI, false);

  /* Setup P-RNTI in PHY */
  add_rnti(SRSLTE_PRNTI, false);

  /* Setup RA-RNTI in PHY */
  for (int i = 0; i < 10; i++) {
    add_rnti(1 + i, false);
  }

  if (srslte_softbuffer_tx_init(&temp_mbsfn_softbuffer, phy->cell.nof_prb)) {
    ERROR("Error initiating soft buffer\n");
    exit(-1);
  }

  srslte_softbuffer_tx_reset(&temp_mbsfn_softbuffer);

  Info("Component Carrier Worker %d configured cell %d PRB\n", cc_idx, phy->cell.nof_prb);

  if (phy->params.pusch_8bit_decoder) {
    enb_ul.pusch.llr_is_8bit        = true;
    enb_ul.pusch.ul_sch.llr_is_8bit = true;
  }
  initiated = true;

#ifdef DEBUG_WRITE_FILE
  f = fopen("test.dat", "w");
#endif
}

void cc_worker::reset()
{
  initiated = false;
  ue_db.clear();
}

cf_t* cc_worker::get_buffer_rx(uint32_t antenna_idx)
{
  return signal_buffer_rx[antenna_idx];
}

cf_t* cc_worker::get_buffer_tx(uint32_t antenna_idx)
{
  return signal_buffer_tx[antenna_idx];
}

void cc_worker::set_tti(uint32_t tti_)
{
  tti_rx    = tti_;
  tti_tx_dl = TTI_TX(tti_rx);
  tti_tx_ul = TTI_RX_ACK(tti_rx);

  t_tx_dl = TTIMOD(tti_tx_dl);
  t_rx    = TTIMOD(tti_rx);
  t_tx_ul = TTIMOD(tti_tx_ul);
}

int cc_worker::add_rnti(uint16_t rnti, bool is_temporal)
{

  if (!is_temporal) {
    if (srslte_enb_dl_add_rnti(&enb_dl, rnti)) {
      return -1;
    }
    if (srslte_enb_ul_add_rnti(&enb_ul, rnti)) {
      return -1;
    }
  }

  mutex.lock();
  // Create user unless already exists
  if (!ue_db.count(rnti)) {
    ue_db[rnti] = new ue(rnti, phy);
  }
  mutex.unlock();

  return SRSLTE_SUCCESS;
}

void cc_worker::rem_rnti(uint16_t rnti)
{
  std::lock_guard<std::mutex> lock(mutex);
  if (ue_db.count(rnti)) {

    delete ue_db[rnti];
    ue_db.erase(rnti);

    srslte_enb_dl_rem_rnti(&enb_dl, rnti);
    srslte_enb_ul_rem_rnti(&enb_ul, rnti);

    // remove any pending dci for each subframe
    for (uint32_t i = 0; i < TTIMOD_SZ; i++) {
      for (uint32_t j = 0; j < phy->ul_grants[i].nof_grants; j++) {
        if (phy->ul_grants[i].pusch[j].dci.rnti == rnti) {
          phy->ul_grants[i].pusch[j].dci.rnti = 0;
        }
      }
      for (uint32_t j = 0; j < phy->dl_grants[i].nof_grants; j++) {
        if (phy->dl_grants[i].pdsch[j].dci.rnti == rnti) {
          phy->dl_grants[i].pdsch[j].dci.rnti = 0;
        }
      }
    }
  } else {
    Error("Removing user: rnti=0x%x does not exist\n", rnti);
  }
}

uint32_t cc_worker::get_nof_rnti()
{
  std::lock_guard<std::mutex> lock(mutex);
  return ue_db.size();
}

void cc_worker::set_config_dedicated(uint16_t rnti, asn1::rrc::phys_cfg_ded_s* dedicated)
{
  std::lock_guard<std::mutex> lock(mutex);

  if (ue_db.count(rnti)) {

    if (dedicated->pusch_cfg_ded_present && dedicated->sched_request_cfg_present) {
      ue_db[rnti]->ul_cfg.pusch.uci_offset.I_offset_ack = dedicated->pusch_cfg_ded.beta_offset_ack_idx;
      ue_db[rnti]->ul_cfg.pusch.uci_offset.I_offset_cqi = dedicated->pusch_cfg_ded.beta_offset_cqi_idx;
      ue_db[rnti]->ul_cfg.pusch.uci_offset.I_offset_ri  = dedicated->pusch_cfg_ded.beta_offset_ri_idx;

      ue_db[rnti]->ul_cfg.pucch.n_pucch_2  = dedicated->cqi_report_cfg.cqi_report_periodic.setup().cqi_pucch_res_idx;
      ue_db[rnti]->ul_cfg.pucch.n_pucch_sr = dedicated->sched_request_cfg.setup().sr_pucch_res_idx;
    }

    if (dedicated->sched_request_cfg_present) {
      ue_db[rnti]->ul_cfg.pucch.I_sr          = dedicated->sched_request_cfg.setup().sr_cfg_idx;
      ue_db[rnti]->ul_cfg.pucch.sr_configured = true;
    }

    /* CQI Reporting */
    if (dedicated->cqi_report_cfg.cqi_report_periodic_present and
        dedicated->cqi_report_cfg.cqi_report_periodic.type() == setup_e::setup) {
      ue_db[rnti]->dl_cfg.cqi_report.periodic_configured = true;
      ue_db[rnti]->dl_cfg.cqi_report.pmi_idx = dedicated->cqi_report_cfg.cqi_report_periodic.setup().cqi_pmi_cfg_idx;
      ue_db[rnti]->ul_cfg.pucch.simul_cqi_ack =
          dedicated->cqi_report_cfg.cqi_report_periodic.setup().simul_ack_nack_and_cqi;
    } else {
      ue_db[rnti]->dl_cfg.cqi_report.pmi_idx = 0;
    }

    /* RI reporting */
    if (dedicated->cqi_report_cfg.cqi_report_periodic.setup().ri_cfg_idx_present) {
      ue_db[rnti]->dl_cfg.cqi_report.ri_idx         = dedicated->cqi_report_cfg.cqi_report_periodic.setup().ri_cfg_idx;
      ue_db[rnti]->dl_cfg.cqi_report.ri_idx_present = true;
    } else {
      ue_db[rnti]->dl_cfg.cqi_report.ri_idx_present = false;
    }

    if (dedicated->ant_info_present) {
      /* If default antenna info then follow 3GPP 36.331 clause 9.2.4 Default physical channel configuration */
      if (dedicated->ant_info.type() == phys_cfg_ded_s::ant_info_c_::types::default_value) {
        if (enb_dl.cell.nof_ports == 1) {
          ue_db[rnti]->dl_cfg.tm = SRSLTE_TM1;
        } else {
          ue_db[rnti]->dl_cfg.tm = SRSLTE_TM2;
        }
        ue_db[rnti]->dl_cfg.cqi_report.ri_idx         = 0;
        ue_db[rnti]->dl_cfg.cqi_report.ri_idx_present = false;
      } else {
        /* Physical channel reconfiguration according to 3GPP 36.331 clause 5.3.10.6 */
        switch (dedicated->ant_info.explicit_value().tx_mode) {
          case ant_info_ded_s::tx_mode_e_::tm1:
            ue_db[rnti]->dl_cfg.cqi_report.ri_idx         = 0;
            ue_db[rnti]->dl_cfg.cqi_report.ri_idx_present = false;
            ue_db[rnti]->dl_cfg.tm                        = SRSLTE_TM1;
            break;
          case ant_info_ded_s::tx_mode_e_::tm2:
            ue_db[rnti]->dl_cfg.cqi_report.ri_idx         = 0;
            ue_db[rnti]->dl_cfg.cqi_report.ri_idx_present = false;
            ue_db[rnti]->dl_cfg.tm                        = SRSLTE_TM2;
            break;
          case ant_info_ded_s::tx_mode_e_::tm3:
            ue_db[rnti]->dl_cfg.tm = SRSLTE_TM3;
            break;
          case ant_info_ded_s::tx_mode_e_::tm4:
            ue_db[rnti]->dl_cfg.tm = SRSLTE_TM4;
            break;
          default:
            ue_db[rnti]->dl_cfg.tm = SRSLTE_TM1;
            Error("TM mode %s not supported\n", dedicated->ant_info.explicit_value().tx_mode.to_string().c_str());
            break;
        }
      }
    }
  } else {
    Error("Setting config dedicated: rnti=0x%x does not exist\n", rnti);
  }
}

void cc_worker::work_ul(srslte_ul_sf_cfg_t* ul_sf_cfg, stack_interface_phy_lte::ul_sched_t* ul_grants)
{
  std::lock_guard<std::mutex> lock(mutex);
  ul_sf = *ul_sf_cfg;
  log_h->step(ul_sf.tti);

  for (auto& ue : ue_db) {
    ue.second->is_grant_available = false;
  }

  // Process UL signal
  srslte_enb_ul_fft(&enb_ul);

  // Decode pending UL grants for the tti they were scheduled
  decode_pusch(ul_grants->pusch, ul_grants->nof_grants);

  // Decode remaining PUCCH ACKs not associated with PUSCH transmission and SR signals
  decode_pucch();
}

void cc_worker::work_dl(srslte_dl_sf_cfg_t*                  dl_sf_cfg,
                        stack_interface_phy_lte::dl_sched_t* dl_grants,
                        stack_interface_phy_lte::ul_sched_t* ul_grants,
                        srslte_mbsfn_cfg_t*                  mbsfn_cfg)
{
  std::lock_guard<std::mutex> lock(mutex);
  dl_sf = *dl_sf_cfg;

  // Put base signals (references, PBCH, PCFICH and PSS/SSS) into the resource grid
  srslte_enb_dl_put_base(&enb_dl, &dl_sf);

  // Put DL grants to resource grid. PDSCH data will be encoded as well.
  if (dl_sf_cfg->sf_type == SRSLTE_SF_NORM) {
    encode_pdcch_dl(dl_grants->pdsch, dl_grants->nof_grants);
    encode_pdsch(dl_grants->pdsch, dl_grants->nof_grants);
  } else {
    if (mbsfn_cfg->enable) {
      encode_pmch(dl_grants->pdsch, mbsfn_cfg);
    }
  }

  // Put UL grants to resource grid.
  encode_pdcch_ul(ul_grants->pusch, ul_grants->nof_grants);

  // Put pending PHICH HARQ ACK/NACK indications into subframe
  encode_phich(ul_grants->phich, ul_grants->nof_phich);

  // Generate signal and transmit
  srslte_enb_dl_gen_signal(&enb_dl);
}

bool cc_worker::fill_uci_cfg(uint16_t rnti, bool aperiodic_cqi_request, srslte_uci_cfg_t* uci_cfg)
{
  bool uci_required = false;

  bzero(uci_cfg, sizeof(srslte_uci_cfg_t));

  // Check if SR opportunity (will only be used in PUCCH)
  uci_cfg->is_scheduling_request_tti = (srslte_ue_ul_sr_send_tti(&ue_db[rnti]->ul_cfg.pucch, tti_rx) == 1);

  uci_required |= uci_cfg->is_scheduling_request_tti;

  // Get pending ACKs with an associated PUSCH transmission
  // TODO: Use ue_dl procedures to compute uci_ack_cfg for TDD and CA
  for (uint32_t tb = 0; tb < SRSLTE_MAX_TB; tb++) {
    uci_cfg->ack[cc_idx].pending_tb[tb] = phy->ue_db_is_ack_pending(tti_rx, rnti, tb, &uci_cfg->ack[cc_idx].ncce[0]);
    Debug("ACK: is pending tti=%d, mod=%d, value=%d\n", tti_rx, TTIMOD(tti_rx), uci_cfg->ack[cc_idx].pending_tb[tb]);
    if (uci_cfg->ack[cc_idx].pending_tb[tb]) {
      uci_cfg->ack[cc_idx].nof_acks++;
      uci_required = true;
    }
  }

  // Get pending CQI reports for this TTI
  if (srslte_enb_dl_gen_cqi_periodic(
          &enb_dl.cell, &ue_db[rnti]->dl_cfg, tti_rx, phy->ue_db_get_ri(rnti), &uci_cfg->cqi)) {
    uci_required = true;
  } else if (aperiodic_cqi_request) {
    srslte_enb_dl_gen_cqi_aperiodic(&enb_dl.cell, &ue_db[rnti]->dl_cfg, phy->ue_db_get_ri(rnti), &uci_cfg->cqi);
    uci_required = true;
  }

  return uci_required;
}

void cc_worker::send_uci_data(uint16_t rnti, srslte_uci_cfg_t* uci_cfg, srslte_uci_value_t* uci_value)
{
  // Notify SR
  if (uci_cfg->is_scheduling_request_tti && uci_value->scheduling_request) {
    phy->stack->sr_detected(tti_rx, rnti);
  }

  /* If only one ACK is required, it can be for TB0 or TB1 */
  uint32_t ack_idx = 0;
  for (uint32_t tb = 0; tb < SRSLTE_MAX_TB; tb++) {
    if (uci_cfg->ack[cc_idx].pending_tb[tb]) {
      bool ack   = uci_value->ack.ack_value[ack_idx];
      bool valid = uci_value->ack.valid;
      phy->stack->ack_info(tti_rx, rnti, tb, ack && valid);
      ack_idx++;
    }
  }

  // Notify CQI only if CRC is valid
  if (uci_value->cqi.data_crc) {
    if (uci_cfg->cqi.data_enable) {
      uint8_t cqi_value = 0;
      switch (uci_cfg->cqi.type) {
        case SRSLTE_CQI_TYPE_WIDEBAND:
          cqi_value = uci_value->cqi.wideband.wideband_cqi;
          break;
        case SRSLTE_CQI_TYPE_SUBBAND:
          cqi_value = uci_value->cqi.subband.subband_cqi;
          break;
        case SRSLTE_CQI_TYPE_SUBBAND_HL:
          cqi_value = uci_value->cqi.subband_hl.wideband_cqi_cw0;
          break;
        case SRSLTE_CQI_TYPE_SUBBAND_UE:
          cqi_value = uci_value->cqi.subband_ue.wideband_cqi;
          break;
      }
      phy->stack->cqi_info(tti_rx, rnti, cqi_value);
    }
    if (uci_cfg->cqi.ri_len) {
      phy->stack->ri_info(tti_rx, rnti, uci_value->ri);
      phy->ue_db_set_ri(rnti, uci_value->ri);
    }
    if (uci_cfg->cqi.pmi_present) {
      uint8_t pmi_value = 0;
      switch (uci_cfg->cqi.type) {
        case SRSLTE_CQI_TYPE_WIDEBAND:
          pmi_value = uci_value->cqi.wideband.pmi;
          break;
        case SRSLTE_CQI_TYPE_SUBBAND_HL:
          pmi_value = uci_value->cqi.subband_hl.pmi;
          break;
        default:
          Error("CQI type=%d not implemented for PMI\n", uci_cfg->cqi.type);
          break;
      }
      phy->stack->pmi_info(tti_rx, rnti, pmi_value);
    }
  }
}

int cc_worker::decode_pusch(stack_interface_phy_lte::ul_sched_grant_t* grants, uint32_t nof_pusch)
{
  srslte_pusch_res_t pusch_res;

  for (uint32_t i = 0; i < nof_pusch; i++) {
    uint16_t rnti = grants[i].dci.rnti;
    if (rnti) {
      // mark this tti as having an ul dci to avoid pucch
      ue_db[rnti]->is_grant_available = true;

      fill_uci_cfg(rnti, grants->dci.cqi_request, &ue_db[rnti]->ul_cfg.pusch.uci_cfg);

      // Compute UL grant
      srslte_pusch_grant_t* grant = &ue_db[rnti]->ul_cfg.pusch.grant;
      if (srslte_ra_ul_dci_to_grant(&phy->cell, &ul_sf, &ue_db[rnti]->ul_cfg.hopping, &grants[i].dci, grant)) {
        Error("Computing PUSCH dci\n");
        return SRSLTE_ERROR;
      }

      uint32_t ul_pid = TTI_RX(ul_sf.tti) % SRSLTE_FDD_NOF_HARQ;

      // Handle Format0 adaptive retx
      // Use last TBS for this TB in case of mcs>28
      if (grants[i].dci.tb.mcs_idx > 28) {
        grant->tb = phy->ue_db_get_last_ul_tb(rnti, ul_pid);
        Info("RETX: mcs=%d, old_tbs=%d pid=%d\n", grants[i].dci.tb.mcs_idx, grant->tb.tbs, ul_pid);
      }
      phy->ue_db_set_last_ul_tb(rnti, ul_pid, grant->tb);

      // Run PUSCH decoder
      pusch_res                                = {};
      ue_db[rnti]->ul_cfg.pusch.softbuffers.rx = grants[i].softbuffer_rx;
      pusch_res.data                           = grants[i].data;
      if (pusch_res.data) {
        if (srslte_enb_ul_get_pusch(&enb_ul, &ul_sf, &ue_db[rnti]->ul_cfg.pusch, &pusch_res)) {
          Error("Decoding PUSCH\n");
          return SRSLTE_ERROR;
        }
      }

      // Save PHICH scheduling for this user. Each user can have just 1 PUSCH dci per TTI
      ue_db[rnti]->phich_grant.n_prb_lowest = grant->n_prb_tilde[0];
      ue_db[rnti]->phich_grant.n_dmrs       = grants[i].dci.n_dmrs;

      float snr_db = enb_ul.chest_res.snr_db;

      // Notify MAC of RL status
      if (snr_db >= PUSCH_RL_SNR_DB_TH) {
        phy->stack->snr_info(ul_sf.tti, rnti, snr_db);

        if (grants[i].dci.tb.rv == 0) {
          if (!pusch_res.crc) {
            Debug("PUSCH: Radio-Link failure snr=%.1f dB\n", snr_db);
            phy->stack->rl_failure(rnti);
          } else {
            phy->stack->rl_ok(rnti);
          }
        }
      }

      // Send UCI data to MAC
      send_uci_data(rnti, &ue_db[rnti]->ul_cfg.pusch.uci_cfg, &pusch_res.uci);

      // Notify MAC new received data and HARQ Indication value
      if (pusch_res.data) {
        phy->stack->crc_info(tti_rx, rnti, grant->tb.tbs / 8, pusch_res.crc);

        // Save metrics stats
        ue_db[rnti]->metrics_ul(grants[i].dci.tb.mcs_idx, 0, snr_db, pusch_res.avg_iterations_block);

        // Logging
        char str[512];
        srslte_pusch_rx_info(&ue_db[rnti]->ul_cfg.pusch, &pusch_res, str, 512);
        Info("PUSCH: %s, snr=%.1f dB\n", str, snr_db);
      }
    }
  }
  return SRSLTE_SUCCESS;
}

int cc_worker::decode_pucch()
{
  srslte_pucch_res_t pucch_res;
  ZERO_OBJECT(pucch_res);

  for (auto& iter : ue_db) {
    auto rnti = (uint16_t)iter.first;

    // If it's a User RNTI and doesn't have PUSCH grant in this TTI
    if (SRSLTE_RNTI_ISUSER(rnti) && !ue_db[rnti]->is_grant_available) {
      // Check if user needs to receive PUCCH
      if (fill_uci_cfg(rnti, false, &ue_db[rnti]->ul_cfg.pucch.uci_cfg)) {
        // Decode PUCCH
        if (srslte_enb_ul_get_pucch(&enb_ul, &ul_sf, &ue_db[rnti]->ul_cfg.pucch, &pucch_res)) {
          ERROR("Error getting PUCCH\n");
          return SRSLTE_ERROR;
        }

        // Notify MAC of RL status (skip SR subframes)
        if (!ue_db[rnti]->ul_cfg.pucch.uci_cfg.is_scheduling_request_tti) {
          if (pucch_res.correlation < PUCCH_RL_CORR_TH) {
            Debug("PUCCH: Radio-Link failure corr=%.1f\n", pucch_res.correlation);
            phy->stack->rl_failure(rnti);
          } else {
            phy->stack->rl_ok(rnti);
          }
        }

        // Send UCI data to MAC
        send_uci_data(rnti, &ue_db[rnti]->ul_cfg.pucch.uci_cfg, &pucch_res.uci_data);

        // Logging
        char str[512];
        srslte_pucch_rx_info(&ue_db[rnti]->ul_cfg.pucch, &pucch_res.uci_data, str, 512);
        Info("PUCCH: %s, corr=%.1f\n", str, pucch_res.correlation);
      }
    }
  }
  return 0;
}

int cc_worker::encode_phich(stack_interface_phy_lte::ul_sched_ack_t* acks, uint32_t nof_acks)
{
  for (uint32_t i = 0; i < nof_acks; i++) {
    if (acks[i].rnti && ue_db.count(acks[i].rnti)) {
      srslte_enb_dl_put_phich(&enb_dl, &ue_db[acks[i].rnti]->phich_grant, acks[i].ack);

      Info("PHICH: rnti=0x%x, hi=%d, I_lowest=%d, n_dmrs=%d, tti_tx_dl=%d\n",
           acks[i].rnti,
           acks[i].ack,
           ue_db[acks[i].rnti]->phich_grant.n_prb_lowest,
           ue_db[acks[i].rnti]->phich_grant.n_dmrs,
           tti_tx_dl);
    }
  }
  return SRSLTE_SUCCESS;
}

int cc_worker::encode_pdcch_ul(stack_interface_phy_lte::ul_sched_grant_t* grants, uint32_t nof_grants)
{
  for (uint32_t i = 0; i < nof_grants; i++) {
    if (grants[i].needs_pdcch) {
      if (srslte_enb_dl_put_pdcch_ul(&enb_dl, &grants[i].dci_cfg, &grants[i].dci)) {
        ERROR("Error putting PUSCH %d\n", i);
        return SRSLTE_ERROR;
      }

      // Logging
      char str[512];
      srslte_dci_ul_info(&grants[i].dci, str, 512);
      Info("PDCCH: %s, tti_tx_dl=%d\n", str, tti_tx_dl);
    }
  }
  return SRSLTE_SUCCESS;
}

int cc_worker::encode_pdcch_dl(stack_interface_phy_lte::dl_sched_grant_t* grants, uint32_t nof_grants)
{
  for (uint32_t i = 0; i < nof_grants; i++) {
    uint16_t rnti = grants[i].dci.rnti;
    if (rnti) {
      if (srslte_enb_dl_put_pdcch_dl(&enb_dl, &grants[i].dci_cfg, &grants[i].dci)) {
        ERROR("Error putting PDCCH %d\n", i);
        return SRSLTE_ERROR;
      }

      if (LOG_THIS(rnti)) {
        // Logging
        char str[512];
        srslte_dci_dl_info(&grants[i].dci, str, 512);
        Info("PDCCH: %s, tti_tx_dl=%d\n", str, tti_tx_dl);
      }
    }
  }
  return 0;
}

int cc_worker::encode_pmch(stack_interface_phy_lte::dl_sched_grant_t* grant, srslte_mbsfn_cfg_t* mbsfn_cfg)
{
  srslte_pmch_cfg_t pmch_cfg;
  ZERO_OBJECT(pmch_cfg);
  srslte_configure_pmch(&pmch_cfg, &phy->cell, mbsfn_cfg);
  srslte_ra_dl_compute_nof_re(&phy->cell, &dl_sf, &pmch_cfg.pdsch_cfg.grant);

  // Set soft buffer
  pmch_cfg.pdsch_cfg.softbuffers.tx[0] = &temp_mbsfn_softbuffer;

  // Encode PMCH
  if (srslte_enb_dl_put_pmch(&enb_dl, &pmch_cfg, grant->data[0])) {
    Error("Error putting PMCH\n");
    return SRSLTE_ERROR;
  }

  // Logging
  char str[512];
  srslte_pdsch_tx_info(&pmch_cfg.pdsch_cfg, str, 512);
  Info("PMCH: %s\n", str);

  // Save metrics stats
  ue_db[SRSLTE_MRNTI]->metrics_dl(mbsfn_cfg->mbsfn_mcs);
  return SRSLTE_SUCCESS;
}

int cc_worker::encode_pdsch(stack_interface_phy_lte::dl_sched_grant_t* grants, uint32_t nof_grants)
{

  /* Scales the Resources Elements affected by the power allocation (p_b) */
  // srslte_enb_dl_prepare_power_allocation(&enb_dl);

  // Prepare for receive ACK for DL grants in t_tx_dl+4
  phy->ue_db_clear(tti_tx_ul);

  for (uint32_t i = 0; i < nof_grants; i++) {
    uint16_t rnti = grants[i].dci.rnti;
    if (rnti) {

      // Compute DL grant
      if (srslte_ra_dl_dci_to_grant(
              &phy->cell, &dl_sf, ue_db[rnti]->dl_cfg.tm, false, &grants[i].dci, &ue_db[rnti]->dl_cfg.pdsch.grant)) {
        Error("Computing DL grant\n");
      }

      // Set soft buffer
      for (uint32_t j = 0; j < SRSLTE_MAX_CODEWORDS; j++) {
        ue_db[rnti]->dl_cfg.pdsch.softbuffers.tx[j] = grants[i].softbuffer_tx[j];
      }

      // Encode PDSCH
      if (srslte_enb_dl_put_pdsch(&enb_dl, &ue_db[rnti]->dl_cfg.pdsch, grants[i].data)) {
        Error("Error putting PDSCH %d\n", i);
        return SRSLTE_ERROR;
      }

      // Save pending ACK
      if (SRSLTE_RNTI_ISUSER(rnti)) {
        /* For each TB */
        for (uint32_t tb_idx = 0; tb_idx < SRSLTE_MAX_TB; tb_idx++) {
          /* If TB enabled, set pending ACK */
          if (ue_db[rnti]->dl_cfg.pdsch.grant.tb[tb_idx].enabled) {
            Debug("ACK: set pending tti=%d, mod=%d\n", tti_tx_ul, TTIMOD(tti_tx_ul));
            phy->ue_db_set_ack_pending(tti_tx_ul, rnti, tb_idx, grants[i].dci.location.ncce);
          }
        }
      }

      if (LOG_THIS(rnti)) {
        // Logging
        char str[512];
        srslte_pdsch_tx_info(&ue_db[rnti]->dl_cfg.pdsch, str, 512);
        Info("PDSCH: %s, tti_tx_dl=%d\n", str, tti_tx_dl);
      }

      // Save metrics stats
      ue_db[rnti]->metrics_dl(grants[i].dci.tb[0].mcs_idx);
    }
  }

  // srslte_enb_dl_apply_power_allocation(&enb_dl);

  return SRSLTE_SUCCESS;
}

/************ METRICS interface ********************/
uint32_t cc_worker::get_metrics(phy_metrics_t metrics[ENB_METRICS_MAX_USERS])
{
  std::lock_guard<std::mutex> lock(mutex);
  uint32_t                    cnt = 0;
  for (auto& ue : ue_db) {
    if ((SRSLTE_RNTI_ISUSER(ue.first) || ue.first == SRSLTE_MRNTI) &&
        cnt < ENB_METRICS_MAX_USERS) {
      ue.second->metrics_read(&metrics[cnt]);
      cnt++;
    }
  }
  return cnt;
}

void cc_worker::ue::metrics_read(phy_metrics_t* metrics_)
{
  if (metrics_) {
    *metrics_ = metrics;
  }
  bzero(&metrics, sizeof(phy_metrics_t));
}

void cc_worker::ue::metrics_dl(uint32_t mcs)
{
  metrics.dl.mcs = SRSLTE_VEC_CMA(mcs, metrics.dl.mcs, metrics.dl.n_samples);
  metrics.dl.n_samples++;
}

void cc_worker::ue::metrics_ul(uint32_t mcs, float rssi, float sinr, float turbo_iters)
{
  metrics.ul.mcs         = SRSLTE_VEC_CMA((float)mcs, metrics.ul.mcs, metrics.ul.n_samples);
  metrics.ul.sinr        = SRSLTE_VEC_CMA((float)sinr, metrics.ul.sinr, metrics.ul.n_samples);
  metrics.ul.rssi        = SRSLTE_VEC_CMA((float)rssi, metrics.ul.rssi, metrics.ul.n_samples);
  metrics.ul.turbo_iters = SRSLTE_VEC_CMA((float)turbo_iters, metrics.ul.turbo_iters, metrics.ul.n_samples);
  metrics.ul.n_samples++;
}

int cc_worker::read_ce_abs(float* ce_abs)
{
  int sz = srslte_symbol_sz(phy->cell.nof_prb);
  bzero(ce_abs, sizeof(float) * sz);
  int g = (sz - 12 * phy->cell.nof_prb) / 2;
  srslte_vec_abs_dB_cf(enb_ul.chest_res.ce, -80.0f, &ce_abs[g], SRSLTE_NRE * phy->cell.nof_prb);
  return sz;
}

int cc_worker::read_ce_arg(float* ce_arg)
{
  int sz = srslte_symbol_sz(phy->cell.nof_prb);
  bzero(ce_arg, sizeof(float) * sz);
  int g = (sz - 12 * phy->cell.nof_prb) / 2;
  srslte_vec_arg_deg_cf(enb_ul.chest_res.ce, -80.0f, &ce_arg[g], SRSLTE_NRE * phy->cell.nof_prb);
  return sz;
}

int cc_worker::read_pusch_d(cf_t* pdsch_d)
{
  int nof_re = enb_ul.pusch.max_re;
  memcpy(pdsch_d, enb_ul.pusch.d, nof_re * sizeof(cf_t));
  return nof_re;
}

int cc_worker::read_pucch_d(cf_t* pdsch_d)
{
  int nof_re = SRSLTE_PUCCH_MAX_BITS / 2;
  memcpy(pdsch_d, enb_ul.pucch.z_tmp, nof_re * sizeof(cf_t));
  return nof_re;
}

} // namespace srsenb

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

#ifndef SRSLTE_DUMMY_CLASSES_H
#define SRSLTE_DUMMY_CLASSES_H

#include "srslte/interfaces/enb_interfaces.h"

namespace srsenb {

class mac_dummy : public mac_interface_rrc
{
public:
  int  cell_cfg(sched_interface::cell_cfg_t* cell_cfg) override { return 0; }
  void reset() override {}
  int  ue_cfg(uint16_t rnti, sched_interface::ue_cfg_t* cfg) override { return 0; }
  int  ue_rem(uint16_t rnti) override { return 0; }
  int  bearer_ue_cfg(uint16_t rnti, uint32_t lc_id, sched_interface::ue_bearer_cfg_t* cfg) override { return 0; }
  int  bearer_ue_rem(uint16_t rnti, uint32_t lc_id) override { return 0; }
  int  set_dl_ant_info(uint16_t rnti, asn1::rrc::phys_cfg_ded_s::ant_info_c_* dl_ant_info) override { return 0; }
  void phy_config_enabled(uint16_t rnti, bool enabled) override {}
  void write_mcch(asn1::rrc::sib_type2_s* sib2, asn1::rrc::sib_type13_r9_s* sib13, asn1::rrc::mcch_msg_s* mcch) override
  {
  }
};

class rlc_dummy : public rlc_interface_rrc
{
public:
  void clear_buffer(uint16_t rnti) override {}
  void add_user(uint16_t rnti) override {}
  void rem_user(uint16_t rnti) override {}
  void add_bearer(uint16_t rnti, uint32_t lcid, srslte::rlc_config_t cnfg) override {}
  void add_bearer_mrb(uint16_t rnti, uint32_t lcid) override {}
  void write_sdu(uint16_t rnti, uint32_t lcid, srslte::unique_byte_buffer_t sdu) override {}
  bool has_bearer(uint16_t rnti, uint32_t lcid) override { return false; }
};

class pdcp_dummy : public pdcp_interface_rrc
{
public:
  void reset(uint16_t rnti) override {}
  void add_user(uint16_t rnti) override {}
  void rem_user(uint16_t rnti) override {}
  void write_sdu(uint16_t rnti, uint32_t lcid, srslte::unique_byte_buffer_t sdu) override {}
  void add_bearer(uint16_t rnti, uint32_t lcid, srslte::pdcp_config_t cnfg) override {}
  void config_security(uint16_t                            rnti,
                       uint32_t                            lcid,
                       uint8_t*                            k_rrc_enc_,
                       uint8_t*                            k_rrc_int_,
                       uint8_t*                            k_up_enc_,
                       srslte::CIPHERING_ALGORITHM_ID_ENUM cipher_algo_,
                       srslte::INTEGRITY_ALGORITHM_ID_ENUM integ_algo_) override
  {
  }
  void enable_integrity(uint16_t rnti, uint32_t lcid) override {}
  void enable_encryption(uint16_t rnti, uint32_t lcid) override {}
  bool get_bearer_status(uint16_t rnti, uint32_t lcid, uint16_t* dlsn, uint16_t* dlhfn, uint16_t* ulsn, uint16_t* ulhfn)
      override
  {
    return true;
  }
};

class s1ap_dummy : public s1ap_interface_rrc
{
public:
  void
  initial_ue(uint16_t rnti, LIBLTE_S1AP_RRC_ESTABLISHMENT_CAUSE_ENUM cause, srslte::unique_byte_buffer_t pdu) override
  {
  }
  void initial_ue(uint16_t                                 rnti,
                  LIBLTE_S1AP_RRC_ESTABLISHMENT_CAUSE_ENUM cause,
                  srslte::unique_byte_buffer_t             pdu,
                  uint32_t                                 m_tmsi,
                  uint8_t                                  mmec) override
  {
  }

  void write_pdu(uint16_t rnti, srslte::unique_byte_buffer_t pdu) override {}
  bool user_exists(uint16_t rnti) override { return true; }
  bool user_release(uint16_t rnti, LIBLTE_S1AP_CAUSERADIONETWORK_ENUM cause_radio) override { return true; }
  void ue_ctxt_setup_complete(uint16_t rnti, LIBLTE_S1AP_MESSAGE_INITIALCONTEXTSETUPRESPONSE_STRUCT* res) override {}
  void ue_erab_setup_complete(uint16_t rnti, LIBLTE_S1AP_MESSAGE_E_RABSETUPRESPONSE_STRUCT* res) override {}
  bool is_mme_connected() override { return true; }
  bool send_ho_required(uint16_t                     rnti,
                        uint32_t                     target_eci,
                        srslte::plmn_id_t            target_plmn,
                        srslte::unique_byte_buffer_t rrc_container) override
  {
    return true;
  }
  bool send_enb_status_transfer_proc(uint16_t rnti, std::vector<bearer_status_info>& bearer_status_list) override
  {
    return true;
  }
};

class phy_dummy : public phy_interface_rrc_lte
{
public:
  void
  configure_mbsfn(asn1::rrc::sib_type2_s* sib2, asn1::rrc::sib_type13_r9_s* sib13, asn1::rrc::mcch_msg_s mcch) override
  {
  }
  void set_config_dedicated(uint16_t rnti, asn1::rrc::phys_cfg_ded_s* dedicated) override {}
};

class gtpu_dummy : public gtpu_interface_rrc
{
public:
  void add_bearer(uint16_t rnti, uint32_t lcid, uint32_t addr, uint32_t teid_out, uint32_t* teid_in) {}
  void rem_bearer(uint16_t rnti, uint32_t lcid) {}
  void rem_user(uint16_t rnti) {}
};

} // namespace srsenb

#endif // SRSLTE_DUMMY_CLASSES_H

/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2015 Software Radio Systems Limited
 *
 * \section LICENSE
 *
 * This file is part of the srsUE library.
 *
 * srsUE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsUE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#ifndef UEPHYWORKERCOMMON_H
#define UEPHYWORKERCOMMON_H

#define TX_MODE_CONTINUOUS 1


#include <pthread.h>
#include <string.h>
#include <vector>
#include "srslte/srslte.h"
#include "srslte/interfaces/ue_interfaces.h"
#include "srslte/radio/radio.h"
#include "srslte/common/log.h"
#include "phy/phy_metrics.h"


namespace srsue {


class chest_feedback_itf
{
public:
  virtual void in_sync() = 0;
  virtual void out_of_sync() = 0;
  virtual void set_cfo(float cfo) = 0;
};

/* Subclass that manages variables common to all workers */
class phch_common {
public:

  /* Common variables used by all phy workers */
  phy_interface_rrc::phy_cfg_t *config;
  phy_args_t                   *args;
  rrc_interface_phy *rrc;
  mac_interface_phy *mac;

  /* Power control variables */
  float pathloss;
  float cur_pathloss;
  float p0_preamble;
  float cur_radio_power;
  float cur_pusch_power;
  float avg_rsrp;
  float avg_rsrp_dbm;
  float avg_rsrq_db;
  float avg_rssi_dbm;
  float last_radio_rssi;
  float rx_gain_offset;
  float avg_snr_db;
  float avg_noise;

  bool     pcell_meas_enabled;
  uint32_t pcell_report_period;

  // Save last TBS for mcs>28 cases
  int last_dl_tbs[2*HARQ_DELAY_MS][SRSLTE_MAX_CODEWORDS];
  uint32_t last_dl_tti[2*HARQ_DELAY_MS];

  int last_ul_tbs[2*HARQ_DELAY_MS];
  uint32_t last_ul_tti[2*HARQ_DELAY_MS];
  srslte_mod_t last_ul_mod[2*HARQ_DELAY_MS];

  uint8_t last_ri;
  uint8_t last_pmi;

  phch_common(uint32_t max_mutex = 3);
  void init(phy_interface_rrc::phy_cfg_t *config,
            phy_args_t  *args,
            srslte::log *_log,
            srslte::radio *_radio,
            rrc_interface_phy *rrc,
            mac_interface_phy *_mac);

  /* For RNTI searches, -1 means now or forever */
  void               set_ul_rnti(srslte_rnti_type_t type, uint16_t rnti_value, int tti_start = -1, int tti_end = -1);
  uint16_t           get_ul_rnti(uint32_t tti);
  srslte_rnti_type_t get_ul_rnti_type();

  void               set_dl_rnti(srslte_rnti_type_t type, uint16_t rnti_value, int tti_start = -1, int tti_end = -1);
  uint16_t           get_dl_rnti(uint32_t tti);
  srslte_rnti_type_t get_dl_rnti_type();

  void set_rar_grant(uint32_t tti, uint8_t grant_payload[SRSLTE_RAR_GRANT_LEN]);
  bool get_pending_rar(uint32_t tti, srslte_dci_rar_grant_t *rar_grant = NULL);

  void reset_pending_ack(uint32_t tti);
  void set_pending_ack(uint32_t tti, uint32_t I_lowest, uint32_t n_dmrs);
  bool get_pending_ack(uint32_t tti);
  bool get_pending_ack(uint32_t tti, uint32_t *I_lowest, uint32_t *n_dmrs);
  bool is_any_pending_ack();

  void worker_end(uint32_t tti, bool tx_enable, cf_t *buffer, uint32_t nof_samples, srslte_timestamp_t tx_time);

  void set_nof_mutex(uint32_t nof_mutex);

  bool sr_enabled;
  int  sr_last_tx_tti;

  srslte::radio*    get_radio();

  void set_cell(const srslte_cell_t &c);
  uint32_t get_nof_prb();
  void set_dl_metrics(const dl_metrics_t &m);
  void get_dl_metrics(dl_metrics_t &m);
  void set_ul_metrics(const ul_metrics_t &m);
  void get_ul_metrics(ul_metrics_t &m);
  void set_sync_metrics(const sync_metrics_t &m);
  void get_sync_metrics(sync_metrics_t &m);

  void reset_ul();
  void reset();

private:

  std::vector<pthread_mutex_t>    tx_mutex;

  bool               is_first_of_burst;
  srslte::radio      *radio_h;
  float              cfo;
  srslte::log       *log_h;


  bool               ul_rnti_active(uint32_t tti);
  bool               dl_rnti_active(uint32_t tti);
  uint16_t           ul_rnti, dl_rnti;
  srslte_rnti_type_t ul_rnti_type, dl_rnti_type;
  int                ul_rnti_start, ul_rnti_end, dl_rnti_start, dl_rnti_end;

  float              time_adv_sec;

  srslte_dci_rar_grant_t rar_grant;
  bool                   rar_grant_pending;
  uint32_t               rar_grant_tti;

  typedef struct {
    bool enabled;
    uint32_t I_lowest;
    uint32_t n_dmrs;
  } pending_ack_t;
  pending_ack_t pending_ack[TTIMOD_SZ];

  bool            is_first_tx;

  uint32_t        nof_workers;
  uint32_t        nof_mutex;
  uint32_t        max_mutex;

  srslte_cell_t   cell;

  dl_metrics_t    dl_metrics;
  uint32_t        dl_metrics_count;
  bool            dl_metrics_read;
  ul_metrics_t    ul_metrics;
  uint32_t        ul_metrics_count;
  bool            ul_metrics_read;
  sync_metrics_t  sync_metrics;
  uint32_t        sync_metrics_count;
  bool            sync_metrics_read;
};
  
} // namespace srsue

#endif // UEPHYWORKERCOMMON_H

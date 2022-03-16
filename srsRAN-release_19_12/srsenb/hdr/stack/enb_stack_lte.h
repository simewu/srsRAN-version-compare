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

/******************************************************************************
 * File:        enb_stack_lte.h
 * Description: L2/L3 LTE eNB stack class.
 *****************************************************************************/

#ifndef SRSLTE_ENB_STACK_LTE_H
#define SRSLTE_ENB_STACK_LTE_H

#include "mac/mac.h"
#include "rrc/rrc.h"
#include "upper/gtpu.h"
#include "upper/pdcp.h"
#include "upper/rlc.h"
#include "upper/s1ap.h"

#include "srslte/common/log_filter.h"

#include "enb_stack_base.h"
#include "srsenb/hdr/enb.h"
#include "srslte/common/multiqueue.h"
#include "srslte/interfaces/enb_interfaces.h"

namespace srsenb {

class enb_stack_lte final : public enb_stack_base,
                            public stack_interface_phy_lte,
                            public stack_interface_s1ap_lte,
                            public stack_interface_gtpu_lte,
                            public stack_interface_mac_lte,
                            public thread
{
public:
  enb_stack_lte(srslte::logger* logger_);
  ~enb_stack_lte() final;

  // eNB stack base interface
  int         init(const stack_args_t& args_, const rrc_cfg_t& rrc_cfg_, phy_interface_stack_lte* phy_);
  int         init(const stack_args_t& args_, const rrc_cfg_t& rrc_cfg_);
  void        stop() final;
  std::string get_type() final;
  bool        get_metrics(stack_metrics_t* metrics) final;

  /* PHY-MAC interface */
  int sr_detected(uint32_t tti, uint16_t rnti) final { return mac.sr_detected(tti, rnti); }
  int rach_detected(uint32_t tti, uint32_t primary_cc_idx, uint32_t preamble_idx, uint32_t time_adv) final
  {
    return mac.rach_detected(tti, primary_cc_idx, preamble_idx, time_adv);
  }
  int ri_info(uint32_t tti, uint16_t rnti, uint32_t ri_value) final { return mac.ri_info(tti, rnti, ri_value); }
  int pmi_info(uint32_t tti, uint16_t rnti, uint32_t pmi_value) final { return mac.pmi_info(tti, rnti, pmi_value); }
  int cqi_info(uint32_t tti, uint16_t rnti, uint32_t cqi_value) final { return mac.cqi_info(tti, rnti, cqi_value); }
  int snr_info(uint32_t tti, uint16_t rnti, float snr_db) final { return mac.snr_info(tti, rnti, snr_db); }
  int ack_info(uint32_t tti, uint16_t rnti, uint32_t tb_idx, bool ack) final
  {
    return mac.ack_info(tti, rnti, tb_idx, ack);
  }
  int crc_info(uint32_t tti, uint16_t rnti, uint32_t nof_bytes, bool crc_res) final
  {
    return mac.crc_info(tti, rnti, nof_bytes, crc_res);
  }
  int get_dl_sched(uint32_t tti, dl_sched_t* dl_sched_res) final { return mac.get_dl_sched(tti, dl_sched_res); }
  int get_mch_sched(uint32_t tti, bool is_mcch, dl_sched_t* dl_sched_res) final
  {
    return mac.get_mch_sched(tti, is_mcch, dl_sched_res);
  }
  int  get_ul_sched(uint32_t tti, ul_sched_t* ul_sched_res) final { return mac.get_ul_sched(tti, ul_sched_res); }
  void set_sched_dl_tti_mask(uint8_t* tti_mask, uint32_t nof_sfs) final
  {
    mac.set_sched_dl_tti_mask(tti_mask, nof_sfs);
  }
  // Radio-Link status
  void rl_failure(uint16_t rnti) final { mac.rl_failure(rnti); }
  void rl_ok(uint16_t rnti) final { mac.rl_ok(rnti); }
  void tti_clock() override;

  /* STACK-S1AP interface*/
  void add_mme_socket(int fd) override;
  void remove_mme_socket(int fd) override;
  void add_gtpu_s1u_socket_handler(int fd) override;
  void add_gtpu_m1u_socket_handler(int fd) override;

  /* Stack-MAC interface */
  void process_pdus() override;

private:
  static const int STACK_MAIN_THREAD_PRIO = -1; // Use default high-priority below UHD
  // thread loop
  void run_thread() override;
  void stop_impl();
  void tti_clock_impl();
  void handle_mme_rx_packet(srslte::unique_byte_buffer_t pdu,
                            const sockaddr_in&           from,
                            const sctp_sndrcvinfo&       sri,
                            int                          flags);

  // args
  stack_args_t args    = {};
  rrc_cfg_t    rrc_cfg = {};

  // components that layers depend on (need to be destroyed after layers)
  srslte::timer_handler                           timers;
  std::unique_ptr<srslte::rx_multisocket_handler> rx_sockets;

  srsenb::mac      mac;
  srslte::mac_pcap mac_pcap;
  srsenb::rlc      rlc;
  srsenb::pdcp     pdcp;
  srsenb::rrc      rrc;
  srsenb::gtpu     gtpu;
  srsenb::s1ap     s1ap;

  srslte::logger*           logger = nullptr;
  srslte::byte_buffer_pool* pool   = nullptr;

  // Radio and PHY log are in enb.cc
  srslte::log_filter mac_log;
  srslte::log_filter rlc_log;
  srslte::log_filter pdcp_log;
  srslte::log_filter rrc_log;
  srslte::log_filter s1ap_log;
  srslte::log_filter gtpu_log;
  srslte::log_filter stack_log;
  srslte::log_filter asn1_log, rrc_asn1_log;

  // RAT-specific interfaces
  phy_interface_stack_lte* phy = nullptr;

  // state
  bool                            started = false;
  srslte::multiqueue_task_handler pending_tasks;
  int enb_queue_id = -1, sync_queue_id = -1, mme_queue_id = -1, gtpu_queue_id = -1, mac_queue_id = -1;
};

} // namespace srsenb

#endif // SRSLTE_ENB_STACK_LTE_H

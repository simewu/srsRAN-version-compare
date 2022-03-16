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

#include "srsenb/hdr/stack/enb_stack_lte.h"
#include "srsenb/hdr/enb.h"
#include "srslte/common/network_utils.h"
#include "srslte/srslte.h"
#include <srslte/interfaces/enb_metrics_interface.h>

using namespace srslte;

namespace srsenb {

enb_stack_lte::enb_stack_lte(srslte::logger* logger_) :
  timers(128),
  logger(logger_),
  pdcp(&timers, &pdcp_log),
  thread("STACK")
{
  enb_queue_id  = pending_tasks.add_queue();
  sync_queue_id = pending_tasks.add_queue();
  mme_queue_id  = pending_tasks.add_queue();
  gtpu_queue_id = pending_tasks.add_queue();
  mac_queue_id  = pending_tasks.add_queue();

  pool = byte_buffer_pool::get_instance();
}

enb_stack_lte::~enb_stack_lte()
{
  stop();
}

std::string enb_stack_lte::get_type()
{
  return "lte";
}

int enb_stack_lte::init(const stack_args_t& args_, const rrc_cfg_t& rrc_cfg_, phy_interface_stack_lte* phy_)
{
  phy = phy_;
  if (init(args_, rrc_cfg_)) {
    return SRSLTE_ERROR;
  }

  return SRSLTE_SUCCESS;
}

int enb_stack_lte::init(const stack_args_t& args_, const rrc_cfg_t& rrc_cfg_)
{
  args    = args_;
  rrc_cfg = rrc_cfg_;

  // setup logging for each layer
  mac_log.init("MAC ", logger, true);
  rlc_log.init("RLC ", logger);
  pdcp_log.init("PDCP", logger);
  rrc_log.init("RRC ", logger);
  gtpu_log.init("GTPU", logger);
  s1ap_log.init("S1AP", logger);
  stack_log.init("STACK", logger);
  asn1_log.init("ASN1", logger);
  rrc_asn1_log.init("ASN1::RRC", logger);

  // Init logs
  mac_log.set_level(args.log.mac_level);
  rlc_log.set_level(args.log.rlc_level);
  pdcp_log.set_level(args.log.pdcp_level);
  rrc_log.set_level(args.log.rrc_level);
  gtpu_log.set_level(args.log.gtpu_level);
  s1ap_log.set_level(args.log.s1ap_level);
  stack_log.set_level(LOG_LEVEL_INFO);
  asn1_log.set_level(LOG_LEVEL_INFO);
  rrc_asn1_log.set_level(args.log.rrc_level);

  mac_log.set_hex_limit(args.log.mac_hex_limit);
  rlc_log.set_hex_limit(args.log.rlc_hex_limit);
  pdcp_log.set_hex_limit(args.log.pdcp_hex_limit);
  rrc_log.set_hex_limit(args.log.rrc_hex_limit);
  gtpu_log.set_hex_limit(args.log.gtpu_hex_limit);
  s1ap_log.set_hex_limit(args.log.s1ap_hex_limit);
  stack_log.set_hex_limit(128);
  asn1_log.set_hex_limit(128);
  asn1::srsasn_log_register_handler(&asn1_log);
  rrc_asn1_log.set_hex_limit(args.log.rrc_hex_limit);
  asn1::rrc::rrc_log_register_handler(&rrc_log);

  // Set up pcap and trace
  if (args.pcap.enable) {
    mac_pcap.open(args.pcap.filename.c_str());
    mac.start_pcap(&mac_pcap);
  }

  // verify configuration correctness
  uint32_t       prach_freq_offset = rrc_cfg.sibs[1].sib2().rr_cfg_common.prach_cfg.prach_cfg_info.prach_freq_offset;
  srslte_cell_t& cell_cfg          = rrc_cfg.cell;
  if (cell_cfg.nof_prb > 10) {
    uint32_t lower_bound = SRSLTE_MAX(rrc_cfg.sr_cfg.nof_prb, rrc_cfg.cqi_cfg.nof_prb);
    uint32_t upper_bound = cell_cfg.nof_prb - lower_bound;
    if (prach_freq_offset + 6 > upper_bound or prach_freq_offset < lower_bound) {
      fprintf(stderr,
              "ERROR: Invalid PRACH configuration - prach_freq_offset=%d collides with PUCCH.\n",
              prach_freq_offset);
      fprintf(stderr,
              "       Consider changing \"prach_freq_offset\" in sib.conf to a value between %d and %d.\n",
              lower_bound,
              upper_bound);
      return SRSLTE_ERROR;
    }
  } else { // 6 PRB case
    if (prach_freq_offset + 6 > cell_cfg.nof_prb) {
      fprintf(stderr,
              "ERROR: Invalid PRACH configuration - prach=(%d, %d) does not fit into the eNB PRBs=(0, %d).\n",
              prach_freq_offset,
              prach_freq_offset + 6,
              cell_cfg.nof_prb);
      fprintf(
          stderr,
          "       Consider changing the \"prach_freq_offset\" value to 0 in the sib.conf file when using 6 PRBs.\n");
      return SRSLTE_ERROR;
    }
  }

  // Init Rx socket handler
  rx_sockets.reset(new srslte::rx_multisocket_handler("ENBSOCKETS", &stack_log));

  // Init all layers
  mac.init(args.mac, &cell_cfg, phy, &rlc, &rrc, this, &mac_log);
  rlc.init(&pdcp, &rrc, &mac, &timers, &rlc_log);
  pdcp.init(&rlc, &rrc, &gtpu);
  rrc.init(&rrc_cfg, phy, &mac, &rlc, &pdcp, &s1ap, &gtpu, &timers, &rrc_log);
  s1ap.init(args.s1ap, &rrc, &s1ap_log, &timers, this);
  gtpu.init(args.s1ap.gtp_bind_addr,
            args.s1ap.mme_addr,
            args.embms.m1u_multiaddr,
            args.embms.m1u_if_addr,
            &pdcp,
            this,
            &gtpu_log,
            args.embms.enable);

  started = true;
  start(STACK_MAIN_THREAD_PRIO);

  return SRSLTE_SUCCESS;
}

void enb_stack_lte::tti_clock()
{
  pending_tasks.push(sync_queue_id, [this]() { tti_clock_impl(); });
}

void enb_stack_lte::tti_clock_impl()
{
  timers.step_all();
  rrc.tti_clock();
}

void enb_stack_lte::stop()
{
  if (started) {
    pending_tasks.push(enb_queue_id, [this]() { stop_impl(); });
    wait_thread_finish();
  }
}

void enb_stack_lte::stop_impl()
{
  rx_sockets->stop();

  s1ap.stop();
  gtpu.stop();
  mac.stop();
  rlc.stop();
  pdcp.stop();
  rrc.stop();

  if (args.pcap.enable) {
    mac_pcap.close();
  }

  // erasing the queues is the last thing, bc we need them to call stop_impl()
  pending_tasks.erase_queue(sync_queue_id);
  pending_tasks.erase_queue(enb_queue_id);
  pending_tasks.erase_queue(mme_queue_id);
  pending_tasks.erase_queue(gtpu_queue_id);
  pending_tasks.erase_queue(mac_queue_id);

  started = false;
}

bool enb_stack_lte::get_metrics(stack_metrics_t* metrics)
{
  mac.get_metrics(metrics->mac);
  rrc.get_metrics(metrics->rrc);
  s1ap.get_metrics(metrics->s1ap);
  return true;
}

void enb_stack_lte::run_thread()
{
  while (started) {
    srslte::move_task_t task{};
    if (pending_tasks.wait_pop(&task) >= 0) {
      task();
    }
  }
}

void enb_stack_lte::handle_mme_rx_packet(srslte::unique_byte_buffer_t pdu,
                                         const sockaddr_in&           from,
                                         const sctp_sndrcvinfo&       sri,
                                         int                          flags)
{
  // Defer the handling of MME packet to eNB stack main thread
  auto task_handler = [this, from, sri, flags](srslte::unique_byte_buffer_t& t) {
    s1ap.handle_mme_rx_msg(std::move(t), from, sri, flags);
  };
  // Defer the handling of MME packet to main stack thread
  pending_tasks.push(mme_queue_id, std::bind(task_handler, std::move(pdu)));
}

void enb_stack_lte::add_mme_socket(int fd)
{
  // Pass MME Rx packet handler functor to socket handler to run in socket thread
  auto mme_rx_handler =
      [this](srslte::unique_byte_buffer_t pdu, const sockaddr_in& from, const sctp_sndrcvinfo& sri, int flags) {
        handle_mme_rx_packet(std::move(pdu), from, sri, flags);
      };
  rx_sockets->add_socket_sctp_pdu_handler(fd, mme_rx_handler);
}

void enb_stack_lte::remove_mme_socket(int fd)
{
  rx_sockets->remove_socket(fd);
}

void enb_stack_lte::add_gtpu_s1u_socket_handler(int fd)
{
  auto gtpu_s1u_handler = [this](srslte::unique_byte_buffer_t pdu, const sockaddr_in& from) {
    auto task_handler = [this, from](srslte::unique_byte_buffer_t& t) {
      gtpu.handle_gtpu_s1u_rx_packet(std::move(t), from);
    };
    pending_tasks.push(gtpu_queue_id, std::bind(task_handler, std::move(pdu)));
  };
  rx_sockets->add_socket_pdu_handler(fd, gtpu_s1u_handler);
}

void enb_stack_lte::add_gtpu_m1u_socket_handler(int fd)
{
  auto gtpu_m1u_handler = [this](srslte::unique_byte_buffer_t pdu, const sockaddr_in& from) {
    auto task_handler = [this, from](srslte::unique_byte_buffer_t& t) {
      gtpu.handle_gtpu_m1u_rx_packet(std::move(t), from);
    };
    pending_tasks.push(gtpu_queue_id, std::bind(task_handler, std::move(pdu)));
  };
  rx_sockets->add_socket_pdu_handler(fd, gtpu_m1u_handler);
}

void enb_stack_lte::process_pdus()
{
  pending_tasks.push(mac_queue_id, [this]() { mac.process_pdus(); });
}

} // namespace srsenb

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

#include <unistd.h>

#include "srslte/common/log.h"
#include "srslte/common/threads.h"
#include "srslte/srslte.h"

#include "srsenb/hdr/phy/sf_worker.h"
#include "srsenb/hdr/phy/txrx.h"

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

namespace srsenb {

txrx::txrx() : thread("TXRX")
{
  /* Do nothing */
}

bool txrx::init(srslte::radio_interface_phy* radio_h_,
                srslte::thread_pool*         workers_pool_,
                phy_common*                  worker_com_,
                prach_worker_pool*           prach_,
                srslte::log*                 log_h_,
                uint32_t                     prio_)
{
  radio_h       = radio_h_;
  log_h         = log_h_;
  workers_pool  = workers_pool_;
  worker_com    = worker_com_;
  prach         = prach_;
  tx_worker_cnt = 0;
  running       = true;

  nof_workers = workers_pool->get_nof_workers();
  worker_com->set_nof_workers(nof_workers);

  if (worker_com->params.dl_channel_args.enable) {
    ul_channel = srslte::channel_ptr(new srslte::channel(worker_com->params.ul_channel_args, 1));
  }

  start(prio_);
  return true;
}

void txrx::stop()
{
  running = false;
  wait_thread_finish();
}

void txrx::run_thread()
{
  sf_worker*         worker                   = nullptr;
  cf_t*              buffer[SRSLTE_MAX_PORTS] = {};
  srslte_timestamp_t rx_time                  = {};
  srslte_timestamp_t tx_time                  = {};
  uint32_t           sf_len                   = SRSLTE_SF_LEN_PRB(worker_com->cell.nof_prb);

  float samp_rate = srslte_sampling_freq_hz(worker_com->cell.nof_prb);
  log_h->console("Setting Sampling frequency %.2f MHz\n", (float)samp_rate / 1000000);

  // Configure radio
  radio_h->set_rx_srate(0, samp_rate);
  radio_h->set_tx_srate(0, samp_rate);

  if (ul_channel) {
    ul_channel->set_srate(samp_rate);
  }

  log_h->info("Starting RX/TX thread nof_prb=%d, sf_len=%d\n", worker_com->cell.nof_prb, sf_len);

  // Set TTI so that first TX is at tti=0
  tti = 10235;

  // Main loop
  while (running) {
    tti    = (tti + 1) % 10240;
    worker = (sf_worker*)workers_pool->wait_worker(tti);
    if (worker) {
      for (uint32_t c = 0, i = 0; c < worker_com->params.nof_carriers; c++) {
        for (uint32_t p = 0; p < worker_com->cell.nof_ports; p++, i++) {
          buffer[i] = worker->get_buffer_rx(c, p);
        }
      }

      radio_h->rx_now(0, buffer, sf_len, &rx_time);

      if (ul_channel) {
        ul_channel->run(buffer, buffer, sf_len, rx_time);
      }

      /* Compute TX time: Any transmission happens in TTI+4 thus advance 4 ms the reception time */
      srslte_timestamp_copy(&tx_time, &rx_time);
      srslte_timestamp_add(&tx_time, 0, TX_DELAY * 1e-3);

      Debug("Settting TTI=%d, tx_mutex=%d, tx_time=%ld:%f to worker %d\n",
            tti,
            tx_worker_cnt,
            tx_time.full_secs,
            tx_time.frac_secs,
            worker->get_id());

      worker->set_time(tti, tx_worker_cnt, tx_time);
      tx_worker_cnt = (tx_worker_cnt + 1) % nof_workers;

      // Trigger phy worker execution
      workers_pool->start_worker(worker);

      // Trigger prach worker execution
      for (uint32_t cc = 0; cc < worker_com->params.nof_carriers; cc++) {
        prach->new_tti(cc, tti, buffer[cc * worker_com->cell.nof_ports]);
      }
    } else {
      // wait_worker() only returns NULL if it's being closed. Quit now to avoid unnecessary loops here
      running = false;
    }
  }
}

} // namespace srsenb

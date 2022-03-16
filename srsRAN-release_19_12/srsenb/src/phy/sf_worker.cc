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

#include "srsenb/hdr/phy/sf_worker.h"

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

/* Define GUI-related things */
#ifdef ENABLE_GUI
#include "srsgui/srsgui.h"
#include <semaphore.h>

#include "srslte/srslte.h"

void       init_plots(srsenb::sf_worker* worker);
pthread_t  plot_thread;
sem_t      plot_sem;
static int plot_worker_id = -1;
#else
#pragma message "Compiling without srsGUI support"
#endif
/*********************************************/

using namespace asn1::rrc;

//#define DEBUG_WRITE_FILE

namespace srsenb {

#ifdef DEBUG_WRITE_FILE
FILE* f;
#endif

void sf_worker::init(phy_common* phy_, srslte::log* log_h_)
{
  phy   = phy_;
  log_h = log_h_;

  // Initialise each component carrier workers
  for (uint32_t i = 0; i < phy->params.nof_carriers; i++) {
    // Create pointer
    auto q = new cc_worker();

    // Initialise
    q->init(phy, log_h, i);

    // Create unique pointer
    cc_workers.push_back(std::unique_ptr<cc_worker>(q));
  }

  if (srslte_softbuffer_tx_init(&temp_mbsfn_softbuffer, phy->cell.nof_prb)) {
    ERROR("Error initiating soft buffer\n");
    exit(-1);
  }

  srslte_softbuffer_tx_reset(&temp_mbsfn_softbuffer);

  Info("Worker %d configured cell %d PRB\n", get_id(), phy->cell.nof_prb);

  initiated = true;
  running   = true;

#ifdef DEBUG_WRITE_FILE
  f = fopen("test.dat", "w");
#endif
}

void sf_worker::stop()
{
  std::lock_guard<std::mutex> lock(work_mutex);
  running = false;
  srslte::thread_pool::worker::stop();
}

cf_t* sf_worker::get_buffer_rx(uint32_t cc_idx, uint32_t antenna_idx)
{
  return cc_workers[cc_idx]->get_buffer_rx(antenna_idx);
}

void sf_worker::set_time(uint32_t tti_, uint32_t tx_worker_cnt_, srslte_timestamp_t tx_time_)
{
  tti_rx    = tti_;
  tti_tx_dl = TTI_TX(tti_rx);
  tti_tx_ul = TTI_RX_ACK(tti_rx);

  t_tx_dl = TTIMOD(tti_tx_dl);
  t_rx    = TTIMOD(tti_rx);
  t_tx_ul = TTIMOD(tti_tx_ul);

  tx_worker_cnt = tx_worker_cnt_;
  srslte_timestamp_copy(&tx_time, &tx_time_);

  for (auto& w : cc_workers) {
    w->set_tti(tti_);
  }
}

int sf_worker::add_rnti(uint16_t rnti, bool is_temporal)
{
  for (auto& w : cc_workers) {
    w->add_rnti(rnti, is_temporal);
  }
  return SRSLTE_SUCCESS;
}

void sf_worker::rem_rnti(uint16_t rnti)
{
  for (auto& w : cc_workers) {
    w->rem_rnti(rnti);
  }
}

uint32_t sf_worker::get_nof_rnti()
{
  return cc_workers[0]->get_nof_rnti();
}

void sf_worker::set_config_dedicated(uint16_t rnti, asn1::rrc::phys_cfg_ded_s* dedicated)
{
  for (auto& w : cc_workers) {
    w->set_config_dedicated(rnti, dedicated);
  }
}

void sf_worker::work_imp()
{
  std::lock_guard<std::mutex> lock(work_mutex);
  cf_t*                       signal_buffer_tx[SRSLTE_MAX_PORTS * SRSLTE_MAX_CARRIERS];

  srslte_ul_sf_cfg_t ul_sf = {};
  srslte_dl_sf_cfg_t dl_sf = {};

  if (!running) {
    return;
  }

  srslte_mbsfn_cfg_t mbsfn_cfg;
  srslte_sf_t        sf_type = phy->is_mbsfn_sf(&mbsfn_cfg, tti_tx_dl) ? SRSLTE_SF_MBSFN : SRSLTE_SF_NORM;

  stack_interface_phy_lte::ul_sched_t* ul_grants = phy->ul_grants;
  stack_interface_phy_lte::dl_sched_t* dl_grants = phy->dl_grants;
  stack_interface_phy_lte*             stack     = phy->stack;

  log_h->step(tti_rx);

  Debug("Worker %d running\n", get_id());

  // Configure UL subframe
  ul_sf.tti = tti_rx;

  // Process UL
  for (auto& w : cc_workers) {
    w->work_ul(&ul_sf, &phy->ul_grants[t_rx]);
  }

  // Get DL scheduling for the TX TTI from MAC

  if (sf_type == SRSLTE_SF_NORM) {
    if (stack->get_dl_sched(tti_tx_dl, &dl_grants[t_tx_dl]) < 0) {
      Error("Getting DL scheduling from MAC\n");
      return;
    }
  } else {
    dl_grants[t_tx_dl].cfi = mbsfn_cfg.non_mbsfn_region_length;
    if (stack->get_mch_sched(tti_tx_dl, mbsfn_cfg.is_mcch, &dl_grants[t_tx_dl])) {
      Error("Getting MCH packets from MAC\n");
      return;
    }
  }

  if (dl_grants[t_tx_dl].cfi < 1 || dl_grants[t_tx_dl].cfi > 3) {
    Error("Invalid CFI=%d\n", dl_grants[t_tx_dl].cfi);
    return;
  }

  // Get UL scheduling for the TX TTI from MAC
  if (stack->get_ul_sched(tti_tx_ul, &ul_grants[t_tx_ul]) < 0) {
    Error("Getting UL scheduling from MAC\n");
    return;
  }

  // Configure DL subframe
  dl_sf.tti              = tti_tx_dl;
  dl_sf.cfi              = dl_grants[t_tx_dl].cfi;
  dl_sf.sf_type          = sf_type;
  dl_sf.non_mbsfn_region = mbsfn_cfg.non_mbsfn_region_length;

  // Process DL
  for (auto& w : cc_workers) {
    w->work_dl(&dl_sf, &phy->dl_grants[t_tx_dl], &phy->ul_grants[t_tx_ul], &mbsfn_cfg);
  }

  // Get Transmission buffers
  for (uint32_t cc = 0, i = 0; cc < phy->params.nof_carriers; cc++) {
    for (uint32_t ant = 0; ant < phy->cell.nof_ports; ant++, i++) {
      signal_buffer_tx[i] = cc_workers[cc]->get_buffer_tx(ant);
    }
  }

  Debug("Sending to radio\n");
  phy->worker_end(tx_worker_cnt, signal_buffer_tx, SRSLTE_SF_LEN_PRB(phy->cell.nof_prb), tx_time);

#ifdef DEBUG_WRITE_FILE
  fwrite(signal_buffer_tx, SRSLTE_SF_LEN_PRB(phy->cell.nof_prb) * sizeof(cf_t), 1, f);
#endif

#ifdef DEBUG_WRITE_FILE
  if (tti_tx_dl == 10) {
    fclose(f);
    exit(-1);
  }
#endif

  /* Tell the plotting thread to draw the plots */
#ifdef ENABLE_GUI
  if ((int)get_id() == plot_worker_id) {
    sem_post(&plot_sem);
  }
#endif
}

/************ METRICS interface ********************/
uint32_t sf_worker::get_metrics(phy_metrics_t metrics[ENB_METRICS_MAX_USERS])
{
  uint32_t      cnt                             = 0;
  phy_metrics_t _metrics[ENB_METRICS_MAX_USERS] = {};
  for (uint32_t cc = 0; cc < phy->params.nof_carriers; cc++) {
    cnt = cc_workers[cc]->get_metrics(_metrics);
    for (uint32_t r = 0; r < cnt; r++) {
      phy_metrics_t* m  = &metrics[r];
      phy_metrics_t* _m = &_metrics[r];
      m->dl.mcs         = SRSLTE_VEC_PMA(m->dl.mcs, m->dl.n_samples, _m->dl.mcs, _m->dl.n_samples);
      m->dl.n_samples += _m->dl.n_samples;
      m->ul.n           = SRSLTE_VEC_PMA(m->ul.n, m->ul.n_samples, _m->ul.n, _m->ul.n_samples);
      m->ul.sinr        = SRSLTE_VEC_PMA(m->ul.sinr, m->ul.n_samples, _m->ul.sinr, _m->ul.n_samples);
      m->ul.mcs         = SRSLTE_VEC_PMA(m->ul.mcs, m->ul.n_samples, _m->ul.mcs, _m->ul.n_samples);
      m->ul.rssi        = SRSLTE_VEC_PMA(m->ul.rssi, m->ul.n_samples, _m->ul.rssi, _m->ul.n_samples);
      m->ul.turbo_iters = SRSLTE_VEC_PMA(m->ul.turbo_iters, m->ul.n_samples, _m->ul.turbo_iters, _m->ul.n_samples);
      m->ul.n_samples += _m->ul.n_samples;
    }
  }
  return cnt;
}

void sf_worker::start_plot()
{
#ifdef ENABLE_GUI
  if (plot_worker_id == -1) {
    plot_worker_id = get_id();
    log_h->console("Starting plot for worker_id=%d\n", plot_worker_id);
    init_plots(this);
  } else {
    log_h->console("Trying to start a plot but already started by worker_id=%d\n", plot_worker_id);
  }
#else
  log_h->console("Trying to start a plot but plots are disabled (ENABLE_GUI constant in sf_worker.cc)\n");
#endif
}

int sf_worker::read_ce_abs(float* ce_abs)
{
  return cc_workers[0]->read_ce_abs(ce_abs);
}

int sf_worker::read_ce_arg(float* ce_arg)
{
  return cc_workers[0]->read_ce_arg(ce_arg);
}

int sf_worker::read_pusch_d(cf_t* pdsch_d)
{
  return cc_workers[0]->read_pusch_d(pdsch_d);
}

int sf_worker::read_pucch_d(cf_t* pdsch_d)
{
  return cc_workers[0]->read_pucch_d(pdsch_d);
}

sf_worker::~sf_worker()
{
  srslte_softbuffer_tx_free(&temp_mbsfn_softbuffer);
}

} // namespace srsenb

/***********************************************************
 *
 * PLOT TO VISUALIZE THE CHANNEL RESPONSEE
 *
 ***********************************************************/

#ifdef ENABLE_GUI
plot_real_t    pce, pce_arg;
plot_scatter_t pconst;
plot_scatter_t pconst2;
#define SCATTER_PUSCH_BUFFER_LEN (20 * 6 * SRSLTE_SF_LEN_RE(SRSLTE_MAX_PRB, SRSLTE_CP_NORM))
float tmp_plot[SCATTER_PUSCH_BUFFER_LEN];
float tmp_plot_arg[SCATTER_PUSCH_BUFFER_LEN];
cf_t  tmp_plot2[SRSLTE_SF_LEN_RE(SRSLTE_MAX_PRB, SRSLTE_CP_NORM)];
cf_t  tmp_pucch_plot[SRSLTE_PUCCH_MAX_BITS / 2];

void* plot_thread_run(void* arg)
{
  auto worker = (srsenb::sf_worker*)arg;

  sdrgui_init_title("srsENB");
  plot_real_init(&pce);
  plot_real_setTitle(&pce, (char*)"Channel Response - Magnitude");
  plot_real_setLabels(&pce, (char*)"Index", (char*)"dB");
  plot_real_setYAxisScale(&pce, -40, 40);

  plot_real_init(&pce_arg);
  plot_real_setTitle(&pce_arg, (char*)"Channel Response - Argument");
  plot_real_setLabels(&pce_arg, (char*)"Angle", (char*)"deg");
  plot_real_setYAxisScale(&pce_arg, -180, 180);

  plot_scatter_init(&pconst);
  plot_scatter_setTitle(&pconst, (char*)"PUSCH - Equalized Symbols");
  plot_scatter_setXAxisScale(&pconst, -4, 4);
  plot_scatter_setYAxisScale(&pconst, -4, 4);

  plot_scatter_init(&pconst2);
  plot_scatter_setTitle(&pconst2, (char*)"PUCCH - Equalized Symbols");
  plot_scatter_setXAxisScale(&pconst2, -4, 4);
  plot_scatter_setYAxisScale(&pconst2, -4, 4);

  plot_real_addToWindowGrid(&pce, (char*)"srsenb", 0, 0);
  plot_real_addToWindowGrid(&pce_arg, (char*)"srsenb", 1, 0);
  plot_scatter_addToWindowGrid(&pconst, (char*)"srsenb", 0, 1);
  plot_scatter_addToWindowGrid(&pconst2, (char*)"srsenb", 1, 1);

  int n, n_arg, n_pucch;
  while (true) {
    sem_wait(&plot_sem);

    n       = worker->read_pusch_d(tmp_plot2);
    n_pucch = worker->read_pucch_d(tmp_pucch_plot);
    plot_scatter_setNewData(&pconst, tmp_plot2, n);
    plot_scatter_setNewData(&pconst2, tmp_pucch_plot, n_pucch);

    n = worker->read_ce_abs(tmp_plot);
    plot_real_setNewData(&pce, tmp_plot, n);

    n_arg = worker->read_ce_arg(tmp_plot_arg);
    plot_real_setNewData(&pce_arg, tmp_plot_arg, n_arg);
  }
  return nullptr;
}

void init_plots(srsenb::sf_worker* worker)
{

  if (sem_init(&plot_sem, 0, 0)) {
    perror("sem_init");
    exit(-1);
  }

  pthread_attr_t     attr;
  struct sched_param param = {};
  param.sched_priority     = 0;
  pthread_attr_init(&attr);
  pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
  pthread_attr_setschedparam(&attr, &param);
  if (pthread_create(&plot_thread, &attr, plot_thread_run, worker)) {
    perror("pthread_create");
    exit(-1);
  }
}
#endif

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

#include "srsenb/hdr/stack/mac/scheduler_grid.h"
#include "srsenb/hdr/stack/mac/scheduler.h"
#include <srslte/interfaces/sched_interface.h>

#define Error(fmt, ...) log_h->error(fmt, ##__VA_ARGS__)
#define Warning(fmt, ...) log_h->warning(fmt, ##__VA_ARGS__)
#define Info(fmt, ...) log_h->info(fmt, ##__VA_ARGS__)
#define Debug(fmt, ...) log_h->debug(fmt, ##__VA_ARGS__)

namespace srsenb {

const char* alloc_outcome_t::to_string() const
{
  switch (result) {
    case SUCCESS:
      return "success";
    case DCI_COLLISION:
      return "dci_collision";
    case RB_COLLISION:
      return "rb_collision";
    case ERROR:
      return "error";
  }
  return "unknown error";
}

tti_params_t::tti_params_t(uint32_t tti_rx_) :
  tti_rx(tti_rx_),
  sf_idx(TTI_TX(tti_rx) % 10),
  tti_tx_dl(TTI_TX(tti_rx)),
  tti_tx_ul(TTI_RX_ACK(tti_rx)),
  sfn(TTI_TX(tti_rx) / 10)
{
}

/*******************************************************
 *             PDCCH Allocation Methods
 *******************************************************/

void pdcch_grid_t::init(const sched_params_t& sched_params_)
{
  sched_params = &sched_params_;
  log_h        = sched_params_.log_h;

  reset();
}

void pdcch_grid_t::new_tti(const tti_params_t& tti_params_, uint32_t start_cfi)
{
  tti_params   = &tti_params_;
  current_cfix = start_cfi - 1;
  reset();
}

const sched_ue::sched_dci_cce_t* pdcch_grid_t::get_cce_loc_table(alloc_type_t alloc_type, sched_ue* user) const
{
  switch (alloc_type) {
    case alloc_type_t::DL_BC:
      return &sched_params->common_locations[current_cfix];
    case alloc_type_t::DL_PCCH:
      return &sched_params->common_locations[current_cfix];
    case alloc_type_t::DL_RAR:
      return &sched_params->rar_locations[current_cfix][tti_params->sf_idx];
    case alloc_type_t::DL_DATA:
      return user->get_locations(current_cfix + 1, tti_params->sf_idx);
    case alloc_type_t::UL_DATA:
      return user->get_locations(current_cfix + 1, tti_params->sf_idx);
  }
  return nullptr;
}

bool pdcch_grid_t::alloc_dci(alloc_type_t alloc_type, uint32_t aggr_idx, sched_ue* user)
{
  // TODO: Make the alloc tree update lazy

  /* Get DCI Location Table */
  const sched_ue::sched_dci_cce_t* dci_locs = get_cce_loc_table(alloc_type, user);
  if (dci_locs == nullptr) {
    return false;
  }

  /* Search for potential DCI positions */
  if (prev_end > 0) {
    for (size_t j = prev_start; j < prev_end; ++j) {
      update_alloc_tree((int)j, aggr_idx, user, alloc_type, dci_locs);
    }
  } else {
    update_alloc_tree(-1, aggr_idx, user, alloc_type, dci_locs);
  }

  // if no pdcch space was available
  if (dci_alloc_tree.size() == prev_end) {
    return false;
  }

  prev_start = prev_end;
  prev_end   = dci_alloc_tree.size();

  nof_dci_allocs++;

  return true;
}

void pdcch_grid_t::update_alloc_tree(int                              parent_node_idx,
                                     uint32_t                         aggr_idx,
                                     sched_ue*                        user,
                                     alloc_type_t                     alloc_type,
                                     const sched_ue::sched_dci_cce_t* dci_locs)
{
  alloc_t alloc;
  alloc.rnti      = (user != nullptr) ? user->get_rnti() : (uint16_t)0u;
  alloc.dci_pos.L = aggr_idx;

  // get cumulative pdcch mask
  pdcch_mask_t cum_mask;
  if (parent_node_idx >= 0) {
    cum_mask = dci_alloc_tree[parent_node_idx].second.total_mask;
  } else {
    cum_mask.resize(nof_cces());
  }

  uint32_t nof_locs = dci_locs->nof_loc[aggr_idx];
  for (uint32_t i = 0; i < nof_locs; ++i) {
    uint32_t startpos = dci_locs->cce_start[aggr_idx][i];

    if (alloc_type == alloc_type_t::DL_DATA and user->pucch_sr_collision(tti_params->tti_tx_dl, startpos)) {
      // will cause a collision in the PUCCH
      continue;
    }

    pdcch_mask_t alloc_mask(nof_cces());
    alloc_mask.fill(startpos, startpos + (1u << aggr_idx));
    if ((cum_mask & alloc_mask).any()) {
      // there is collision. Try another mask
      continue;
    }

    // Allocation successful
    alloc.current_mask = alloc_mask;
    alloc.total_mask   = cum_mask | alloc_mask;
    alloc.dci_pos.ncce = startpos;

    // Prune if repetition
    uint32_t j = prev_end;
    for (; j < dci_alloc_tree.size(); ++j) {
      if (dci_alloc_tree[j].second.total_mask == alloc.total_mask) {
        break;
      }
    }
    if (j < dci_alloc_tree.size()) {
      continue;
    }

    // Register allocation
    dci_alloc_tree.emplace_back(parent_node_idx, alloc);
  }
}

bool pdcch_grid_t::set_cfi(uint32_t cfi)
{
  current_cfix = cfi - 1;
  // TODO: use this function for dynamic cfi
  // TODO: The estimation of the number of required prbs in metric depends on CFI. Analyse the consequences
  return true;
}

uint32_t pdcch_grid_t::nof_cces() const
{
  return sched_params->nof_cce_table[current_cfix];
}

void pdcch_grid_t::reset()
{
  prev_start = 0;
  prev_end   = 0;
  dci_alloc_tree.clear();
  nof_dci_allocs = 0;
}

void pdcch_grid_t::get_allocs(alloc_result_t* vec, pdcch_mask_t* tot_mask, size_t idx) const
{
  // if alloc tree is empty
  if (prev_start == prev_end) {
    if (vec != nullptr) {
      vec->clear();
    }
    if (tot_mask != nullptr) {
      tot_mask->reset();
    }
    return;
  }

  // set vector of allocations
  if (vec != nullptr) {
    vec->clear();
    size_t i = prev_start + idx;
    while (dci_alloc_tree[i].first >= 0) {
      vec->push_back(&dci_alloc_tree[i].second);
      i = (size_t)dci_alloc_tree[i].first;
    }
    vec->push_back(&dci_alloc_tree[i].second);
    std::reverse(vec->begin(), vec->end());
  }

  // set final cce mask
  if (tot_mask != nullptr) {
    *tot_mask = dci_alloc_tree[prev_start + idx].second.total_mask;
  }
}

std::string pdcch_grid_t::result_to_string(bool verbose) const
{
  std::stringstream ss;
  ss << "cfi=" << get_cfi() << ", mask_size=" << nof_cces() << ", " << prev_end - prev_start
     << " DCI allocation combinations:\n";
  // get all the possible combinations of DCI allocations
  uint32_t count = 0;
  for (size_t i = prev_start; i < prev_end; ++i) {
    alloc_result_t vec;
    pdcch_mask_t   tot_mask;
    get_allocs(&vec, &tot_mask, i - prev_start);

    ss << "                          combination " << count << ": mask=0x" << tot_mask.to_hex().c_str();
    if (verbose) {
      ss << ", DCI allocs:\n";
      for (const auto& dci_alloc : vec) {
        char hex[5];
        sprintf(hex, "%x", dci_alloc->rnti);
        ss << "                          > rnti=0x" << hex << ": " << dci_alloc->current_mask.to_hex().c_str() << " / "
           << dci_alloc->total_mask.to_hex().c_str() << "\n";
      }
    } else {
      ss << "\n";
    }
    count++;
  }

  return ss.str();
}

/*******************************************************
 *          TTI resource Scheduling Methods
 *******************************************************/

void tti_grid_t::init(const sched_params_t& sched_params_, uint32_t cc_idx_)
{
  sched_params = &sched_params_;
  log_h        = sched_params->log_h;
  nof_rbgs     = sched_params->nof_rbgs;
  si_n_rbg     = srslte::ceil_div(4, sched_params->P);
  rar_n_rbg    = srslte::ceil_div(3, sched_params->P);
  cc_idx       = cc_idx_;

  pdcch_alloc.init(*sched_params);
}

void tti_grid_t::new_tti(const tti_params_t& tti_params_, uint32_t start_cfi)
{
  tti_params = &tti_params_;

  // internal state
  avail_rbg = nof_rbgs;
  dl_mask.reset();
  dl_mask.resize(nof_rbgs);
  ul_mask.reset();
  ul_mask.resize(sched_params->cfg->cell.nof_prb);
  pdcch_alloc.new_tti(*tti_params, start_cfi);
}

//! Allocates CCEs and RBs for the given mask and allocation type (e.g. data, BC, RAR, paging)
alloc_outcome_t tti_grid_t::alloc_dl(uint32_t aggr_lvl, alloc_type_t alloc_type, rbgmask_t alloc_mask, sched_ue* user)
{
  // Check RBG collision
  if ((dl_mask & alloc_mask).any()) {
    return alloc_outcome_t::RB_COLLISION;
  }

  // Allocate DCI in PDCCH
  if (not pdcch_alloc.alloc_dci(alloc_type, aggr_lvl, user)) {
    if (log_h->get_level() == srslte::LOG_LEVEL_DEBUG) {
      if (user != nullptr) {
        log_h->debug("No space in PDCCH for rnti=0x%x DL tx. Current PDCCH allocation: %s\n",
                     user->get_rnti(),
                     pdcch_alloc.result_to_string(true).c_str());
      }
    }
    return alloc_outcome_t::DCI_COLLISION;
  }

  // Allocate RBGs
  dl_mask |= alloc_mask;
  avail_rbg -= alloc_mask.count();

  return alloc_outcome_t::SUCCESS;
}

//! Allocates CCEs and RBs for control allocs. It allocates RBs in a contiguous manner.
tti_grid_t::dl_ctrl_alloc_t tti_grid_t::alloc_dl_ctrl(uint32_t aggr_lvl, alloc_type_t alloc_type)
{
  rbg_range_t range;
  range.rbg_start = nof_rbgs - avail_rbg;
  range.rbg_end   = range.rbg_start + ((alloc_type == alloc_type_t::DL_RAR) ? rar_n_rbg : si_n_rbg);

  if (alloc_type != alloc_type_t::DL_RAR and alloc_type != alloc_type_t::DL_BC and
      alloc_type != alloc_type_t::DL_PCCH) {
    log_h->error("SCHED: DL control allocations must be RAR/BC/PDCCH\n");
    return {alloc_outcome_t::ERROR, range};
  }
  // Setup range starting from left
  if (range.rbg_end > nof_rbgs) {
    return {alloc_outcome_t::RB_COLLISION, range};
  }

  // allocate DCI and RBGs
  rbgmask_t new_mask(dl_mask.size());
  new_mask.fill(range.rbg_start, range.rbg_end);
  return {alloc_dl(aggr_lvl, alloc_type, new_mask), range};
}

//! Allocates CCEs and RBs for a user DL data alloc.
alloc_outcome_t tti_grid_t::alloc_dl_data(sched_ue* user, const rbgmask_t& user_mask)
{
  srslte_dci_format_t dci_format = user->get_dci_format();
  uint32_t            nof_bits   = srslte_dci_format_sizeof(&sched_params->cfg->cell, nullptr, nullptr, dci_format);
  uint32_t            aggr_level = user->get_ue_carrier(cc_idx)->get_aggr_level(nof_bits);
  return alloc_dl(aggr_level, alloc_type_t::DL_DATA, user_mask, user);
}

alloc_outcome_t tti_grid_t::alloc_ul_data(sched_ue* user, ul_harq_proc::ul_alloc_t alloc, bool needs_pdcch)
{
  if (alloc.RB_start + alloc.L > ul_mask.size()) {
    return alloc_outcome_t::ERROR;
  }

  prbmask_t newmask(ul_mask.size());
  newmask.fill(alloc.RB_start, alloc.RB_start + alloc.L);
  if ((ul_mask & newmask).any()) {
    return alloc_outcome_t::RB_COLLISION;
  }

  // Generate PDCCH except for RAR and non-adaptive retx
  if (needs_pdcch) {
    uint32_t nof_bits = srslte_dci_format_sizeof(&sched_params->cfg->cell, nullptr, nullptr, SRSLTE_DCI_FORMAT0);
    uint32_t aggr_idx = user->get_ue_carrier(cc_idx)->get_aggr_level(nof_bits);
    if (not pdcch_alloc.alloc_dci(alloc_type_t::UL_DATA, aggr_idx, user)) {
      if (log_h->get_level() == srslte::LOG_LEVEL_DEBUG) {
        log_h->debug("No space in PDCCH for rnti=0x%x UL tx. Current PDCCH allocation: %s\n",
                     user->get_rnti(),
                     pdcch_alloc.result_to_string(true).c_str());
      }
      return alloc_outcome_t::DCI_COLLISION;
    }
  }

  ul_mask |= newmask;

  return alloc_outcome_t::SUCCESS;
}

/*******************************************************
 *          TTI resource Scheduling Methods
 *******************************************************/

void tti_sched_result_t::init(const sched_params_t& sched_params_, uint32_t enb_cc_idx_)
{
  sched_params = &sched_params_;
  enb_cc_idx   = enb_cc_idx_;
  log_h        = sched_params->log_h;
  tti_alloc.init(*sched_params, 0);
}

void tti_sched_result_t::new_tti(uint32_t tti_rx_, uint32_t start_cfi)
{
  tti_params = tti_params_t{tti_rx_};
  tti_alloc.new_tti(tti_params, start_cfi);

  // internal state
  rar_allocs.clear();
  bc_allocs.clear();
  data_allocs.clear();
  ul_data_allocs.clear();

  // TTI result
  pdcch_mask.reset();
  pdcch_mask.resize(tti_alloc.get_pdcch_grid().nof_cces());
  bzero(&dl_sched_result, sizeof(dl_sched_result));
  bzero(&ul_sched_result, sizeof(ul_sched_result));
}

bool tti_sched_result_t::is_dl_alloc(sched_ue* user) const
{
  for (const auto& a : data_allocs) {
    if (a.user_ptr == user) {
      return true;
    }
  }
  return false;
}

bool tti_sched_result_t::is_ul_alloc(sched_ue* user) const
{
  for (const auto& a : ul_data_allocs) {
    if (a.user_ptr == user) {
      return true;
    }
  }
  return false;
}

tti_sched_result_t::ctrl_code_t tti_sched_result_t::alloc_dl_ctrl(uint32_t aggr_lvl, uint32_t tbs_bytes, uint16_t rnti)
{
  ctrl_alloc_t ctrl_alloc{};

  // based on rnti, check which type of alloc
  alloc_type_t alloc_type = alloc_type_t::DL_RAR;
  if (rnti == SRSLTE_SIRNTI) {
    alloc_type = alloc_type_t::DL_BC;
  } else if (rnti == SRSLTE_PRNTI) {
    alloc_type = alloc_type_t::DL_PCCH;
  }

  /* Allocate space in the DL RBG and PDCCH grids */
  tti_grid_t::dl_ctrl_alloc_t ret = tti_alloc.alloc_dl_ctrl(aggr_lvl, alloc_type);
  if (not ret.outcome) {
    return {ret.outcome, ctrl_alloc};
  }

  // Allocation Successful
  ctrl_alloc.dci_idx    = tti_alloc.get_pdcch_grid().nof_allocs() - 1;
  ctrl_alloc.rbg_range  = ret.rbg_range;
  ctrl_alloc.rnti       = rnti;
  ctrl_alloc.req_bytes  = tbs_bytes;
  ctrl_alloc.alloc_type = alloc_type;

  return {ret.outcome, ctrl_alloc};
}

alloc_outcome_t tti_sched_result_t::alloc_bc(uint32_t aggr_lvl, uint32_t sib_idx, uint32_t sib_ntx)
{
  uint32_t    sib_len = sched_params->cfg->sibs[sib_idx].len;
  uint32_t    rv      = sched::get_rvidx(sib_ntx);
  ctrl_code_t ret     = alloc_dl_ctrl(aggr_lvl, sib_len, SRSLTE_SIRNTI);
  if (not ret.first) {
    Warning("SCHED: Could not allocate SIB=%d, L=%d, len=%d, cause=%s\n",
            sib_idx + 1,
            aggr_lvl,
            sib_len,
            ret.first.to_string());
    return ret.first;
  }

  // BC allocation successful
  bc_alloc_t bc_alloc(ret.second);
  bc_alloc.rv      = rv;
  bc_alloc.sib_idx = sib_idx;
  bc_allocs.push_back(bc_alloc);

  return ret.first;
}

alloc_outcome_t tti_sched_result_t::alloc_paging(uint32_t aggr_lvl, uint32_t paging_payload)
{
  ctrl_code_t ret = alloc_dl_ctrl(aggr_lvl, paging_payload, SRSLTE_PRNTI);
  if (not ret.first) {
    Warning(
        "SCHED: Could not allocate Paging with payload length=%d, cause=%s\n", paging_payload, ret.first.to_string());
    return ret.first;
  }

  // Paging allocation successful
  bc_alloc_t bc_alloc(ret.second);
  bc_allocs.push_back(bc_alloc);

  return ret.first;
}

tti_sched_result_t::rar_code_t tti_sched_result_t::alloc_rar(uint32_t                               aggr_lvl,
                                                             const sched_interface::dl_sched_rar_t& rar_grant,
                                                             uint32_t                               prach_tti,
                                                             uint32_t                               buf_rar)
{
  // RA-RNTI = 1 + t_id + f_id
  // t_id = index of first subframe specified by PRACH (0<=t_id<10)
  // f_id = index of the PRACH within subframe, in ascending order of freq domain (0<=f_id<6) (for FDD, f_id=0)
  uint16_t ra_rnti = 1 + (uint16_t)(prach_tti % 10);

  ctrl_code_t ret = alloc_dl_ctrl(aggr_lvl, buf_rar, ra_rnti);
  if (not ret.first) {
    Warning("SCHED: Could not allocate RAR for L=%d, cause=%s\n", aggr_lvl, ret.first.to_string());
    return {ret.first, nullptr};
  }

  // Allocation successful
  rar_alloc_t rar_alloc(ret.second);
  rar_alloc.rar_grant = rar_grant;
  rar_allocs.push_back(rar_alloc);

  return {ret.first, &rar_allocs.back()};
}

alloc_outcome_t tti_sched_result_t::alloc_dl_user(sched_ue* user, const rbgmask_t& user_mask, uint32_t pid)
{
  if (is_dl_alloc(user)) {
    log_h->warning("SCHED: Attempt to assign multiple harq pids to the same user rnti=0x%x\n", user->get_rnti());
    return alloc_outcome_t::ERROR;
  }

  // Try to allocate RBGs and DCI
  alloc_outcome_t ret = tti_alloc.alloc_dl_data(user, user_mask);
  if (ret != alloc_outcome_t::SUCCESS) {
    return ret;
  }

  // Allocation Successful
  dl_alloc_t alloc;
  alloc.dci_idx   = tti_alloc.get_pdcch_grid().nof_allocs() - 1;
  alloc.user_ptr  = user;
  alloc.user_mask = user_mask;
  alloc.pid       = pid;
  data_allocs.push_back(alloc);

  return alloc_outcome_t::SUCCESS;
}

alloc_outcome_t tti_sched_result_t::alloc_ul(sched_ue*                              user,
                                             ul_harq_proc::ul_alloc_t               alloc,
                                             tti_sched_result_t::ul_alloc_t::type_t alloc_type,
                                             uint32_t                               mcs)
{
  // Check whether user was already allocated
  if (is_ul_alloc(user)) {
    log_h->warning("SCHED: Attempt to assign multiple ul_harq_proc to the same user rnti=0x%x\n", user->get_rnti());
    return alloc_outcome_t::ERROR;
  }

  // Allocate RBGs and DCI space
  bool            needs_pdcch = alloc_type == ul_alloc_t::ADAPT_RETX or alloc_type == ul_alloc_t::NEWTX;
  alloc_outcome_t ret         = tti_alloc.alloc_ul_data(user, alloc, needs_pdcch);
  if (ret != alloc_outcome_t::SUCCESS) {
    return ret;
  }

  ul_alloc_t ul_alloc = {};
  ul_alloc.type       = alloc_type;
  ul_alloc.dci_idx    = tti_alloc.get_pdcch_grid().nof_allocs() - 1;
  ul_alloc.user_ptr   = user;
  ul_alloc.alloc      = alloc;
  ul_alloc.mcs        = mcs;
  ul_data_allocs.push_back(ul_alloc);

  return alloc_outcome_t::SUCCESS;
}

alloc_outcome_t tti_sched_result_t::alloc_ul_user(sched_ue* user, ul_harq_proc::ul_alloc_t alloc)
{
  // check whether adaptive/non-adaptive retx/newtx
  tti_sched_result_t::ul_alloc_t::type_t alloc_type;
  ul_harq_proc* h        = user->get_ul_harq(get_tti_tx_ul(), user->get_cell_index(enb_cc_idx).second);
  bool          has_retx = h->has_pending_retx();
  if (has_retx) {
    ul_harq_proc::ul_alloc_t prev_alloc = h->get_alloc();
    if (prev_alloc.L == alloc.L and prev_alloc.RB_start == prev_alloc.L) {
      alloc_type = ul_alloc_t::NOADAPT_RETX;
    } else {
      alloc_type = ul_alloc_t::ADAPT_RETX;
    }
  } else {
    alloc_type = ul_alloc_t::NEWTX;
  }

  return alloc_ul(user, alloc, alloc_type);
}

alloc_outcome_t tti_sched_result_t::alloc_ul_msg3(sched_ue* user, ul_harq_proc::ul_alloc_t alloc, uint32_t mcs)
{
  return alloc_ul(user, alloc, ul_alloc_t::MSG3, mcs);
}

void tti_sched_result_t::set_bc_sched_result(const pdcch_grid_t::alloc_result_t& dci_result)
{
  for (const auto& bc_alloc : bc_allocs) {
    sched_interface::dl_sched_bc_t* bc = &dl_sched_result.bc[dl_sched_result.nof_bc_elems];

    // assign NCCE/L
    bc->dci.location = dci_result[bc_alloc.dci_idx]->dci_pos;

    /* Generate DCI format1A */
    prb_range_t prb_range = prb_range_t(bc_alloc.rbg_range, sched_params->P);
    int         tbs       = generate_format1a(
        prb_range.prb_start, prb_range.length(), bc_alloc.req_bytes, bc_alloc.rv, bc_alloc.rnti, &bc->dci);

    // Setup BC/Paging processes
    if (bc_alloc.alloc_type == alloc_type_t::DL_BC) {
      if (tbs <= (int)bc_alloc.req_bytes) {
        log_h->warning("SCHED: Error SIB%d, rbgs=(%d,%d), dci=(%d,%d), len=%d\n",
                       bc_alloc.sib_idx + 1,
                       bc_alloc.rbg_range.rbg_start,
                       bc_alloc.rbg_range.rbg_end,
                       bc->dci.location.L,
                       bc->dci.location.ncce,
                       bc_alloc.req_bytes);
        continue;
      }

      // Setup BC process
      bc->index = bc_alloc.sib_idx;
      bc->type  = sched_interface::dl_sched_bc_t::BCCH;
      bc->tbs   = (uint32_t)bc_alloc.req_bytes;

      log_h->info("SCHED: SIB%d, rbgs=(%d,%d), dci=(%d,%d), rv=%d, len=%d, period=%d, mcs=%d\n",
                  bc_alloc.sib_idx + 1,
                  bc_alloc.rbg_range.rbg_start,
                  bc_alloc.rbg_range.rbg_end,
                  bc->dci.location.L,
                  bc->dci.location.ncce,
                  bc_alloc.rv,
                  bc_alloc.req_bytes,
                  sched_params->cfg->sibs[bc_alloc.sib_idx].period_rf,
                  bc->dci.tb[0].mcs_idx);
    } else {
      // Paging
      if (tbs <= 0) {
        log_h->warning("SCHED: Error Paging, rbgs=(%d,%d), dci=(%d,%d)\n",
                       bc_alloc.rbg_range.rbg_start,
                       bc_alloc.rbg_range.rbg_end,
                       bc->dci.location.L,
                       bc->dci.location.ncce);
        continue;
      }

      // Setup Paging process
      bc->type = sched_interface::dl_sched_bc_t::PCCH;
      bc->tbs  = (uint32_t)tbs;

      log_h->info("SCHED: PCH, rbgs=(%d,%d), dci=(%d,%d), tbs=%d, mcs=%d\n",
                  bc_alloc.rbg_range.rbg_start,
                  bc_alloc.rbg_range.rbg_end,
                  bc->dci.location.L,
                  bc->dci.location.ncce,
                  tbs,
                  bc->dci.tb[0].mcs_idx);
    }

    dl_sched_result.nof_bc_elems++;
  }
}

void tti_sched_result_t::set_rar_sched_result(const pdcch_grid_t::alloc_result_t& dci_result)
{
  for (const auto& rar_alloc : rar_allocs) {
    sched_interface::dl_sched_rar_t* rar = &dl_sched_result.rar[dl_sched_result.nof_rar_elems];

    // Assign NCCE/L
    rar->dci.location = dci_result[rar_alloc.dci_idx]->dci_pos;

    /* Generate DCI format1A */
    prb_range_t prb_range = prb_range_t(rar_alloc.rbg_range, sched_params->P);
    int         tbs =
        generate_format1a(prb_range.prb_start, prb_range.length(), rar_alloc.req_bytes, 0, rar_alloc.rnti, &rar->dci);
    if (tbs <= 0) {
      log_h->warning("SCHED: Error RAR, ra_rnti_idx=%d, rbgs=(%d,%d), dci=(%d,%d)\n",
                     rar_alloc.rnti,
                     rar_alloc.rbg_range.rbg_start,
                     rar_alloc.rbg_range.rbg_end,
                     rar->dci.location.L,
                     rar->dci.location.ncce);
      continue;
    }

    // Setup RAR process
    rar->tbs        = rar_alloc.req_bytes;
    rar->nof_grants = rar_alloc.rar_grant.nof_grants;
    std::copy(&rar_alloc.rar_grant.msg3_grant[0], &rar_alloc.rar_grant.msg3_grant[rar->nof_grants], rar->msg3_grant);

    // Print RAR allocation result
    for (uint32_t i = 0; i < rar->nof_grants; ++i) {
      const auto& msg3_grant    = rar->msg3_grant[i];
      uint16_t    expected_rnti = msg3_grant.data.temp_crnti;
      log_h->info("SCHED: RAR, temp_crnti=0x%x, ra-rnti=%d, rbgs=(%d,%d), dci=(%d,%d), rar_grant_rba=%d, "
                  "rar_grant_mcs=%d\n",
                  expected_rnti,
                  rar_alloc.rnti,
                  rar_alloc.rbg_range.rbg_start,
                  rar_alloc.rbg_range.rbg_end,
                  rar->dci.location.L,
                  rar->dci.location.ncce,
                  msg3_grant.grant.rba,
                  msg3_grant.grant.trunc_mcs);
    }

    dl_sched_result.nof_rar_elems++;
  }
}

void tti_sched_result_t::set_dl_data_sched_result(const pdcch_grid_t::alloc_result_t& dci_result)
{
  for (const auto& data_alloc : data_allocs) {
    sched_interface::dl_sched_data_t* data = &dl_sched_result.data[dl_sched_result.nof_data_elems];

    // Assign NCCE/L
    data->dci.location = dci_result[data_alloc.dci_idx]->dci_pos;

    // Generate DCI Format1/2/2A
    sched_ue*           user        = data_alloc.user_ptr;
    uint32_t            cell_index  = user->get_cell_index(enb_cc_idx).second;
    dl_harq_proc*       h           = user->get_dl_harq(data_alloc.pid, cell_index);
    uint32_t            data_before = user->get_pending_dl_new_data();
    srslte_dci_format_t dci_format  = user->get_dci_format();
    bool                is_newtx    = h->is_empty();

    int tbs = 0;
    switch (dci_format) {
      case SRSLTE_DCI_FORMAT1:
        tbs = user->generate_format1(h, data, get_tti_tx_dl(), cell_index, get_cfi(), data_alloc.user_mask);
        break;
      case SRSLTE_DCI_FORMAT2:
        tbs = user->generate_format2(h, data, get_tti_tx_dl(), cell_index, get_cfi(), data_alloc.user_mask);
        break;
      case SRSLTE_DCI_FORMAT2A:
        tbs = user->generate_format2a(h, data, get_tti_tx_dl(), cell_index, get_cfi(), data_alloc.user_mask);
        break;
      default:
        Error("DCI format (%d) not implemented\n", dci_format);
    }

    if (tbs <= 0) {
      log_h->warning("SCHED: Error DL %s rnti=0x%x, pid=%d, mask=%s, tbs=%d, buffer=%d\n",
                     is_newtx ? "tx" : "retx",
                     user->get_rnti(),
                     h->get_id(),
                     data_alloc.user_mask.to_hex().c_str(),
                     tbs,
                     user->get_pending_dl_new_data());
      continue;
    }

    // Print Resulting DL Allocation
    log_h->info("SCHED: DL %s rnti=0x%x, pid=%d, mask=0x%s, dci=(%d,%d), n_rtx=%d, tbs=%d, buffer=%d/%d\n",
                !is_newtx ? "retx" : "tx",
                user->get_rnti(),
                h->get_id(),
                data_alloc.user_mask.to_hex().c_str(),
                data->dci.location.L,
                data->dci.location.ncce,
                h->nof_retx(0) + h->nof_retx(1),
                tbs,
                data_before,
                user->get_pending_dl_new_data());

    dl_sched_result.nof_data_elems++;
  }
}

void tti_sched_result_t::set_ul_sched_result(const pdcch_grid_t::alloc_result_t& dci_result)
{
  /* Set UL data DCI locs and format */
  for (const auto& ul_alloc : ul_data_allocs) {
    sched_interface::ul_sched_data_t* pusch = &ul_sched_result.pusch[ul_sched_result.nof_dci_elems];

    sched_ue* user       = ul_alloc.user_ptr;
    uint32_t  cell_index = user->get_cell_index(enb_cc_idx).second;

    srslte_dci_location_t cce_range = {0, 0};
    if (ul_alloc.needs_pdcch()) {
      cce_range = dci_result[ul_alloc.dci_idx]->dci_pos;
    }

    /* Set fixed mcs if specified */
    int fixed_mcs = (ul_alloc.type == ul_alloc_t::MSG3) ? ul_alloc.mcs : -1;

    /* Generate DCI Format1A */
    uint32_t pending_data_before = user->get_pending_ul_new_data(get_tti_tx_ul());
    int      tbs                 = user->generate_format0(
        pusch, get_tti_tx_ul(), cell_index, ul_alloc.alloc, ul_alloc.needs_pdcch(), cce_range, fixed_mcs);

    ul_harq_proc* h = user->get_ul_harq(get_tti_tx_ul(), cell_index);
    if (tbs <= 0) {
      log_h->warning("SCHED: Error %s %s rnti=0x%x, pid=%d, dci=(%d,%d), prb=(%d,%d), tbs=%d, bsr=%d\n",
                     ul_alloc.type == ul_alloc_t::MSG3 ? "Msg3" : "UL",
                     ul_alloc.is_retx() ? "retx" : "tx",
                     user->get_rnti(),
                     h->get_id(),
                     pusch->dci.location.L,
                     pusch->dci.location.ncce,
                     ul_alloc.alloc.RB_start,
                     ul_alloc.alloc.RB_start + ul_alloc.alloc.L,
                     tbs,
                     user->get_pending_ul_new_data(get_tti_tx_ul()));
      continue;
    }

    // Allocation was successful
    if (ul_alloc.type == ul_alloc_t::NEWTX) {
      // Un-trigger SR
      user->unset_sr();
    }

    // Print Resulting UL Allocation
    log_h->info("SCHED: %s %s rnti=0x%x, pid=%d, dci=(%d,%d), prb=(%d,%d), n_rtx=%d, tbs=%d, bsr=%d (%d-%d)\n",
                ul_alloc.is_msg3() ? "Msg3" : "UL",
                ul_alloc.is_retx() ? "retx" : "tx",
                user->get_rnti(),
                h->get_id(),
                pusch->dci.location.L,
                pusch->dci.location.ncce,
                ul_alloc.alloc.RB_start,
                ul_alloc.alloc.RB_start + ul_alloc.alloc.L,
                h->nof_retx(0),
                tbs,
                user->get_pending_ul_new_data(get_tti_tx_ul()),
                pending_data_before,
                user->get_pending_ul_old_data(cell_index));

    ul_sched_result.nof_dci_elems++;
  }
}

void tti_sched_result_t::generate_dcis()
{
  /* Pick one of the possible DCI masks */
  pdcch_grid_t::alloc_result_t dci_result;
  //  tti_alloc.get_pdcch_grid().result_to_string();
  tti_alloc.get_pdcch_grid().get_allocs(&dci_result, &pdcch_mask);

  /* Register final CFI */
  dl_sched_result.cfi = tti_alloc.get_pdcch_grid().get_cfi();

  /* Generate DCI formats and fill sched_result structs */
  set_bc_sched_result(dci_result);

  set_rar_sched_result(dci_result);

  set_dl_data_sched_result(dci_result);

  set_ul_sched_result(dci_result);
}

uint32_t tti_sched_result_t::get_nof_ctrl_symbols() const
{
  return tti_alloc.get_cfi() + ((sched_params->cfg->cell.nof_prb <= 10) ? 1 : 0);
}

int tti_sched_result_t::generate_format1a(uint32_t         rb_start,
                                          uint32_t         l_crb,
                                          uint32_t         tbs_bytes,
                                          uint32_t         rv,
                                          uint16_t         rnti,
                                          srslte_dci_dl_t* dci)
{
  /* Calculate I_tbs for this TBS */
  int tbs = tbs_bytes * 8;
  int i;
  int mcs = -1;
  for (i = 0; i < 27; i++) {
    if (srslte_ra_tbs_from_idx(i, 2) >= tbs) {
      dci->type2_alloc.n_prb1a = srslte_ra_type2_t::SRSLTE_RA_TYPE2_NPRB1A_2;
      mcs                      = i;
      tbs                      = srslte_ra_tbs_from_idx(i, 2);
      break;
    }
    if (srslte_ra_tbs_from_idx(i, 3) >= tbs) {
      dci->type2_alloc.n_prb1a = srslte_ra_type2_t::SRSLTE_RA_TYPE2_NPRB1A_3;
      mcs                      = i;
      tbs                      = srslte_ra_tbs_from_idx(i, 3);
      break;
    }
  }
  if (i == 28) {
    Error("Can't allocate Format 1A for TBS=%d\n", tbs);
    return -1;
  }

  Debug("ra_tbs=%d/%d, tbs_bytes=%d, tbs=%d, mcs=%d\n",
        srslte_ra_tbs_from_idx(mcs, 2),
        srslte_ra_tbs_from_idx(mcs, 3),
        tbs_bytes,
        tbs,
        mcs);

  dci->alloc_type       = SRSLTE_RA_ALLOC_TYPE2;
  dci->type2_alloc.mode = srslte_ra_type2_t::SRSLTE_RA_TYPE2_LOC;
  dci->type2_alloc.riv  = srslte_ra_type2_to_riv(l_crb, rb_start, sched_params->cfg->cell.nof_prb);
  dci->pid              = 0;
  dci->tb[0].mcs_idx    = mcs;
  dci->tb[0].rv         = rv;
  dci->format           = SRSLTE_DCI_FORMAT1A;
  dci->rnti             = rnti;

  return tbs;
}

} // namespace srsenb

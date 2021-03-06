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

#ifndef RRC_COMMON_H
#define RRC_COMMON_H

namespace srsue {


// RRC states (3GPP 36.331 v10.0.0)
typedef enum {
  RRC_STATE_IDLE = 0,
  RRC_STATE_PLMN_START,
  RRC_STATE_PLMN_SELECTION,
  RRC_STATE_CELL_SELECTING,
  RRC_STATE_CELL_SELECTED,
  RRC_STATE_CONNECTING,
  RRC_STATE_CONNECTED,
  RRC_STATE_HO_PREPARE,
  RRC_STATE_HO_PROCESS,
  RRC_STATE_LEAVE_CONNECTED,
  RRC_STATE_N_ITEMS,
} rrc_state_t;
static const char rrc_state_text[RRC_STATE_N_ITEMS][100] = {"IDLE",
                                                            "PLMN SELECTED",
                                                            "PLMN SELECTION",
                                                            "CELL SELECTING",
                                                            "CELL SELECTED",
                                                            "CONNECTING",
                                                            "CONNECTED",
                                                            "HO PREPARE",
                                                            "HO PROCESS",
                                                            "LEAVE CONNECTED"};

} // namespace srsue


#endif // RRC_COMMON_H

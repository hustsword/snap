/*
 * Copyright 2019 International Business Machines
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include "unistd.h"
#include "libsnap.h"
#include <snap_hls_if.h>

#include "misc_utils.h"
#include "constants.h"


// CAPI basic operations
void action_write (struct snap_card* h, uint32_t addr, uint32_t data, int id);
uint32_t action_read (struct snap_card* h, uint32_t addr, int id);
int action_wait_idle (struct snap_card* h, int timeout);
void soft_reset (struct snap_card* h, int id);
void print_control_status (struct snap_card* h, int id);
struct snap_action* get_action (struct snap_card* handle,
                                snap_action_flag_t flags, int timeout);

// CAPI regex operations
int action_regex (struct snap_card* h,
                  void* patt_src_base,
                  void* pkt_src_base,
                  void* stat_dest_base,
                  size_t* num_matched_pkt,
                  size_t patt_size,
                  size_t pkt_size,
                  size_t stat_size,
                  int id);
int capi_regex_scan_internal (struct snap_card* dnc,
                              int timeout,
                              void* patt_src_base,
                              void* pkt_src_base,
                              void* stat_dest_base,
                              size_t* num_matched_pkt,
                              size_t patt_size,
                              size_t pkt_size,
                              size_t stat_size,
                              int id);
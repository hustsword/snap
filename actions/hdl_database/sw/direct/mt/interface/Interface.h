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

#ifndef INTERFACE_H
#define INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif
#include "db_direct.h"
enum test_mode {POLL = 0, INTERRUPT, INVALID};
int start_regex_workers (int num_engines,
	                 int num_job_per_thd,
			 void* patt_src_base,
			 size_t patt_size,
			 size_t pkt_size,
			 void** job_pkt_src_bases,
			 size_t* job_pkt_sizes,
			 int file_line_count,
			 struct snap_card* dn,
			 struct snap_action* act,
			 snap_action_flag_t attach_flags,
			 float* thd_scan_bw,
			 float* wkr_bw,
			 float* total_bw,
			 uint64_t* max_buff,
			 float* sd_buff,
			 uint64_t* max_scan,
			 float* sd_scan,
			 uint64_t* cleanup_time);
#ifdef __cplusplus
}
#endif

#endif // INTERFACE_H

/*
 * Copyright 2017 International Business Machines
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

#ifndef __SNAP_FW_EXA__
#define __SNAP_FW_EXA__

#include "card_io.h"
#include "misc_utils.h"
#include "constants.h"


int get_file_line_count (FILE* fp);
void remove_newline (char* str);

void* fill_one_packet (const char* in_pkt, int size, void* in_pkt_addr, int in_pkt_id);
void* fill_one_pattern (const char* in_patt, void* in_patt_addr);

int regex_scan (struct snap_card* dnc,
                int timeout,
                void* patt_src_base,
                void* pkt_src_base,
                void* stat_dest_base,
                size_t* num_matched_pkt,
                size_t patt_size,
                size_t pkt_size,
                size_t stat_size,
                int eng_id);


void* sm_compile_file (const char* file_path, size_t* size);
void* regex_scan_file (const char* file_path, size_t* size, size_t* size_for_sw, int num_jobs, void** job_pkt_src_bases, size_t* job_sizes, int* pkt_count);
int compare_num_matched_pkt (size_t num_matched_pkt);
int compare_result_id (uint32_t result_id);

#endif  /* __SNAP_FW_EXA__ */

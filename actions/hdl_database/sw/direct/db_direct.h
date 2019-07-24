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

#include "unistd.h"
#include "libsnap.h"

/*
 * This makes it obvious that we are influenced by HLS details ...
 * The ACTION control bits are defined in the following file.
 */
#include <snap_hls_if.h>

#include "constants.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/time.h>
#include <getopt.h>
#include <ctype.h>

#include <libsnap.h>
#include <snap_tools.h>
#include <snap_s_regs.h>

/*
// Postgresql specific headers
#include "postgres.h"
#include <float.h>
#include <math.h>
#include "fmgr.h"
#include "miscadmin.h"
#include "lib/stringinfo.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "storage/bufpage.h"
#include "access/htup_details.h"
#include "catalog/catalog.h"
#include "catalog/namespace.h"
#include "catalog/pg_type.h"
#include "catalog/pg_class.h"
#include "storage/bufmgr.h"
#include "storage/checksum.h"
#include "utils/pg_lsn.h"
#include "utils/rel.h"
#include "access/relscan.h"
#include "access/heapam.h"
#include "utils/snapmgr.h"
#include "utils/lsyscache.h"
*/

/*  defaults */
#define STEP_DELAY      200
#define DEFAULT_MEMCPY_BLOCK    4096
#define DEFAULT_MEMCPY_ITER 1
#define ACTION_WAIT_TIME    10   /* Default in sec */
//#define MAX_NUM_PKT 502400
//#define MAX_NUM_PKT 4096
#define MIN_NUM_PKT 4096
#define MAX_NUM_PATT 1024

#define MEGAB       (1024*1024ull)
#define GIGAB       (1024 * MEGAB)

/*
typedef struct CAPIContext_s {
    // CAPI device name
    char device[64];
    // CAPI-SNAP card handler
    struct snap_card* dn;
    // Card number
    int card_no;
    // Timeout value before waiting for a card to be valid
    int timeout;
    // Action flags
    snap_action_flag_t attach_flags;
    // Action handler
    struct snap_action* act;
} CAPIContext;

typedef struct CAPIRegexJobDescriptor_s {
    // Pointer to the context
    CAPIContext* context;
    // Pointer to pattern buffer
    void* patt_src_base;
    // Pointer to packet buffer
    void* pkt_src_base;
    // Pointer to destination buffer (result buffer)
    void* stat_dest_base;
    // Number of total packets
    size_t num_pkt;
    // Number of matched packets
    size_t num_matched_pkt;
    // Size of the packet buffer
    size_t pkt_size;
    // Maximum allocated size of the packet buffer
    size_t max_alloc_pkt_size;
    // Size of the pattern buffer
    size_t patt_size;
    // Size of the packet buffer - hardware headers
    size_t pkt_size_wo_hw_hdr;
    // Size of the output buffer
    size_t stat_size;
    // C string to the pattern
    // TODO: currently only 1 pattern for each job
    char* pattern;

    // The pointer to the results (id of matched packets)
    uint32_t* results;

    // An index to record the result processing.
    // Each id corresponds one entry in the results buffer.
    int curr_result_id;

    // The ID of the first block for this job
    //int start_blk_id;

    // Number of blocks (postgresql blocks) for this job
    //int num_blks;

    // The ID of the first tuple for this job
    //int start_tuple_id;

    // Number of tuples (postgresql tuples) for this job
    //size_t num_tuples;

    // The thread id of this job descriptor
    int thread_id;

    // Perf statistics (in nano seconds);
    int64_t t_init;
    int64_t t_regex_patt; // Pattern compile time
    int64_t t_regex_pkt; // The whole pkt preparation time
    int64_t t_regex_pkt_copy; // only the memcpy time
    int64_t t_regex_scan;
    int64_t t_regex_harvest;
    int64_t t_cleanup;

    // Pointer to next descriptor
    void* next_desc;
} CAPIRegexJobDescriptor;
*/

#define PERF_MEASURE(_func, out) \
    do { \
        struct timespec t_beg, t_end; \
        clock_gettime(CLOCK_REALTIME, &t_beg); \
        ERROR_CHECK((_func)); \
        clock_gettime(CLOCK_REALTIME, &t_end); \
        (out) = diff_time (&t_beg, &t_end); \
    } \
    while (0)

#define ERROR_LOG(_file, _func, _line, _rc) \
    do { \
        print_error((_file), (_func), (const char*) (_line), (_rc)); \
    } \
    while (0)

#define ERROR_CHECK(_err) \
    do { \
        int rc = (_err); \
        if (rc != 0) \
        { \
            ERROR_LOG (__FILE__, __FUNCTION__, __LINE__, rc); \
            goto fail; \
        } \
    } while (0)

void print_error (const char* file, const char* func, const char* line, int rc);
int64_t diff_time (struct timespec* t_beg, struct timespec* t_end);
uint64_t get_usec (void);
int get_file_line_count (FILE* fp);
void remove_newline (char* str);
void print_time (uint64_t elapsed, uint64_t size);
void* alloc_mem (int align, size_t size);
void free_mem (void* a);

void* fill_one_packet (const char* in_pkt, int size, void* in_pkt_addr);
void* fill_one_pattern (const char* in_patt, void* in_patt_addr);

void action_write (struct snap_card* h, uint32_t addr, uint32_t data);
uint32_t action_read (struct snap_card* h, uint32_t addr);
int action_wait_idle (struct snap_card* h, int timeout, uint64_t* elapsed);
void print_control_status (struct snap_card* h, int eng_id);
void soft_reset (struct snap_card* h, int eng_id);
void action_regex (struct snap_card* h,
                       void* patt_src_base,
                       void* pkt_src_base,
                       void* stat_dest_base,
                       size_t* num_matched_pkt,
                       size_t patt_size,
                       size_t pkt_size,
                       size_t stat_size,
                       int eng_id);
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
//int regex_scan (CAPIRegexJobDescriptor* job_desc);
struct snap_action* get_action (struct snap_card* handle,
                                       snap_action_flag_t flags, int timeout);

void* sm_compile_file (const char* file_path, size_t* size);
void* regex_scan_file (const char* file_path, size_t* size, size_t* size_for_sw);
int compare_results (size_t num_matched_pkt, void* stat_dest_base, int no_chk_offset);

//int capi_regex_context_init (CAPIContext* context);
//int capi_regex_job_init (CAPIRegexJobDescriptor* job_desc,
  //                       CAPIContext* context);

#endif	/* __SNAP_FW_EXA__ */

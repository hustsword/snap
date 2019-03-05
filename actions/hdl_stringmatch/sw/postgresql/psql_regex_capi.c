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

#include "psql_regex_capi.h"
#include "utils/fregex.h"

// Postgresql specific headers
#include "postgres.h"

#include <float.h>
#include <math.h>

#include "fmgr.h"
#include "miscadmin.h"
#include "windowapi.h"
#include "lib/stringinfo.h"
#include "utils/array.h"
#include "utils/builtins.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1 (psql_regex_capi);

extern Datum psql_regex_capi (PG_FUNCTION_ARGS);

typedef struct {
    bool     isdone;
    bool     isnull;
    uint8_t* result;
    /* variable length */
} psql_regex_capi_context;

/*  defaults */
#define STEP_DELAY      200
#define DEFAULT_MEMCPY_BLOCK    4096
#define DEFAULT_MEMCPY_ITER 1
#define ACTION_WAIT_TIME    10   /* Default in sec */
//#define MAX_NUM_PKT 502400
#define MAX_NUM_PKT 4096

#define MEGAB       (1024*1024ull)
#define GIGAB       (1024 * MEGAB)

static uint32_t PATTERN_ID = 0;
static uint32_t PACKET_ID = 0;
static  int verbose_level = 0;

static uint64_t get_usec (void)
{
    struct timeval t;

    gettimeofday (&t, NULL);
    return t.tv_sec * 1000000 + t.tv_usec;
}

static void print_time (uint64_t elapsed, uint64_t size)
{
    int t;
    float fsize = (float)size / (1024 * 1024);
    float ft;

    if (elapsed > 10000) {
        t = (int)elapsed / 1000;
        ft = (1000 / (float)t) * fsize;
        elog (LOG, " end after %d msec (%0.3f MB/sec)\n", t, ft);
    } else {
        t = (int)elapsed;
        ft = (1000000 / (float)t) * fsize;
        elog (LOG, " end after %d usec (%0.3f MB/sec)\n", t, ft);
    }
}

static void* alloc_mem (int align, size_t size)
{
    void* a;
    size_t size2 = size + align;

    elog (DEBUG1, "%s Enter Align: %d Size: %zu\n", __func__, align, size);

    if (posix_memalign ((void**)&a, 4096, size2) != 0) {
        perror ("FAILED: posix_memalign()");
        return NULL;
    }

    elog (DEBUG1, "%s Exit %p\n", __func__, a);
    return a;
}

static void free_mem (void* a)
{
    elog (DEBUG1, "Free Mem %p\n", a);

    if (a) {
        free (a);
    }
}

static void* fill_one_packet (const char* in_pkt, int size, void* in_pkt_addr)
{
    unsigned char* pkt_base_addr = in_pkt_addr;
    int pkt_id;
    uint32_t bytes_used = 0;
    uint16_t pkt_len = size;

    PACKET_ID++;
    // The TAG ID
    pkt_id = PACKET_ID;

    elog (DEBUG2, "PKT[%d] %s len %d\n", pkt_id, in_pkt, pkt_len);

    // The frame header
    for (int i = 0; i < 4; i++) {
        pkt_base_addr[bytes_used] = 0x5A;
        bytes_used ++;
    }

    // The frame size
    pkt_base_addr[bytes_used] = (pkt_len & 0xFF);
    bytes_used ++;
    pkt_base_addr[bytes_used] = 0;
    pkt_base_addr[bytes_used] |= ((pkt_len >> 8) & 0xF);
    bytes_used ++;

    // Skip the reserved bytes
    for (int i = 0; i < 54; i++) {
        pkt_base_addr[bytes_used] = 0;
        bytes_used++;
    }

    for (int i = 0; i < 4 ; i++) {
        pkt_base_addr[bytes_used] = ((pkt_id >> (8 * i)) & 0xFF);
        bytes_used++;
    }

    // The payload
    for (int i = 0; i < pkt_len; i++) {
        pkt_base_addr[bytes_used] = in_pkt[i];
        bytes_used++;
    }

    // Padding to 64 bytes alignment
    bytes_used--;

    do {
        if ((((uint64_t) (pkt_base_addr + bytes_used)) & 0x3F) == 0x3F) { //the last address of the packet stream is 512bit/64byte aligned
            break;
        } else {
            bytes_used ++;
            pkt_base_addr[bytes_used] = 0x00; //padding 8'h00 until the 512bit/64byte alignment
        }

    }   while (1);

    bytes_used++;

    return pkt_base_addr + bytes_used;

}

static void* fill_one_pattern (const char* in_patt, void* in_patt_addr)
{
    unsigned char* patt_base_addr = in_patt_addr;
    int config_len = 0;
    unsigned char config_bytes[PATTERN_WIDTH_BYTES];
    int x;
    uint32_t pattern_id;
    uint16_t patt_byte_cnt;
    uint32_t bytes_used = 0;

    for (x = 0; x < PATTERN_WIDTH_BYTES; x++) {
        config_bytes[x] = 0;
    }

    // Generate pattern ID
    PATTERN_ID ++;
    pattern_id = PATTERN_ID;

    elog (DEBUG1, "PATT[%d] %s\n", pattern_id, in_patt);

    fregex_get_config (in_patt,
                       MAX_TOKEN_NUM,
                       MAX_STATE_NUM,
                       MAX_CHAR_NUM,
                       MAX_CHAR_PER_TOKEN,
                       config_bytes,
                       &config_len,
                       0);

    elog (DEBUG2, "Config length (bits)  %d\n", config_len * 8);
    elog (DEBUG2, "Config length (bytes) %d\n", config_len);

    for (int i = 0; i < 4; i++) {
        patt_base_addr[bytes_used] = 0x5A;
        bytes_used++;
    }

    patt_byte_cnt = (PATTERN_WIDTH_BYTES - 4);
    patt_base_addr[bytes_used] = patt_byte_cnt & 0xFF;
    bytes_used ++;
    patt_base_addr[bytes_used] = (patt_byte_cnt >> 8) & 0x7;
    bytes_used ++;

    for (int i = 0; i < 54; i++) {
        patt_base_addr[bytes_used] = 0x00;
        bytes_used ++;
    }

    // Pattern ID;
    for (int i = 0; i < 4; i++) {
        patt_base_addr[bytes_used] = (pattern_id >> (i * 8)) & 0xFF;
        bytes_used ++;
    }

    memcpy (patt_base_addr + bytes_used, config_bytes, config_len);
    bytes_used += config_len;
    //for (int i = 0; i < config_len; i++) {
    //    patt_base_addr[bytes_used] = config_bytes[i];
    //    bytes_used ++;
    //}

    // Padding to 64 bytes alignment
    bytes_used --;

    do {
        if ((((uint64_t) (patt_base_addr + bytes_used)) & 0x3F) == 0x3F) { //the last address of the packet stream is 512bit/64byte aligned
            break;
        } else {
            bytes_used ++;
            patt_base_addr[bytes_used] = 0x00; //padding 8'h00 until the 512bit/64byte alignment
        }

    } while (1);

    bytes_used ++;

    return patt_base_addr + bytes_used;

}


/* Action or Kernel Write and Read are 32 bit MMIO */
static void action_write (struct snap_card* h, uint32_t addr, uint32_t data)
{
    int rc;

    rc = snap_mmio_write32 (h, (uint64_t)addr, data);

    if (0 != rc) {
        elog (DEBUG1, "Write MMIO 32 Err\n");
    }

    return;
}

static uint32_t action_read (struct snap_card* h, uint32_t addr)
{
    int rc;
    uint32_t data;

    rc = snap_mmio_read32 (h, (uint64_t)addr, &data);

    if (0 != rc) {
        elog (DEBUG1, "Read MMIO 32 Err\n");
    }

    return data;
}

/*
 *  Start Action and wait for Idle.
 */
static int action_wait_idle (struct snap_card* h, int timeout, uint64_t* elapsed)
{
    int rc = ETIME;
    uint64_t t_start;   /* time in usec */
    uint64_t td = 0;    /* Diff time in usec */

    /* FIXME Use struct snap_action and not struct snap_card */
    snap_action_start ((void*)h);

    /* Wait for Action to go back to Idle */
    t_start = get_usec();
    rc = snap_action_completed ((void*)h, NULL, timeout);
    td = get_usec() - t_start;

    if (rc) {
        rc = 0;    /* Good */
    } else {
        elog (DEBUG1, "Error. Timeout while Waiting for Idle\n");
    }

    *elapsed = td;
    return rc;
}

static void print_control_status (struct snap_card* h)
{
    if (verbose_level > 2) {
        uint32_t reg_data;
        elog (DEBUG3, " READ Control and Status Registers: \n");
        reg_data = action_read (h, ACTION_STATUS_L);
        elog (DEBUG3, "       STATUS_L = 0x%x\n", reg_data);
        reg_data = action_read (h, ACTION_STATUS_H);
        elog (DEBUG3, "       STATUS_H = 0x%x\n", reg_data);
        reg_data = action_read (h, ACTION_CONTROL_L);
        elog (DEBUG3, "       CONTROL_L = 0x%x\n", reg_data);
        reg_data = action_read (h, ACTION_CONTROL_H);
        elog (DEBUG3, "       CONTROL_H = 0x%x\n", reg_data);
    }
}

static void soft_reset (struct snap_card* h)
{
    // Status[4] to reset
    action_write (h, ACTION_CONTROL_L, 0x00000010);
    action_write (h, ACTION_CONTROL_H, 0x00000000);
    elog (DEBUG2, " Write ACTION_CONTROL for soft reset! \n");
    action_write (h, ACTION_CONTROL_L, 0x00000000);
    action_write (h, ACTION_CONTROL_H, 0x00000000);
}

static void action_sm (struct snap_card* h,
                       void* patt_src_base,
                       void* pkt_src_base,
                       void* stat_dest_base,
                       size_t* num_matched_pkt,
                       size_t patt_size,
                       size_t pkt_size,
                       size_t stat_size)
{
    uint32_t reg_data;
    //uint64_t start_time;
    //uint64_t elapsed_time;

    elog (DEBUG2, " ------ String Match Start -------- \n");
    elog (DEBUG2, " PATTERN SOURCE ADDR: %p -- SIZE: %d\n", patt_src_base, (int)patt_size);
    elog (DEBUG2, " PACKET  SOURCE ADDR: %p -- SIZE: %d\n", pkt_src_base, (int)pkt_size);
    elog (DEBUG2, " STAT    DEST   ADDR: %p -- SIZE(max): %d\n", stat_dest_base, (int)stat_size);

    elog (DEBUG2, " Start register config! \n");
    print_control_status (h);

    action_write (h, ACTION_PATT_INIT_ADDR_L,
                  (uint32_t) (((uint64_t) patt_src_base) & 0xffffffff));
    action_write (h, ACTION_PATT_INIT_ADDR_H,
                  (uint32_t) ((((uint64_t) patt_src_base) >> 32) & 0xffffffff));
    elog (DEBUG2, " Write ACTION_PATT_INIT_ADDR done! \n");

    action_write (h, ACTION_PKT_INIT_ADDR_L,
                  (uint32_t) (((uint64_t) pkt_src_base) & 0xffffffff));
    action_write (h, ACTION_PKT_INIT_ADDR_H,
                  (uint32_t) ((((uint64_t) pkt_src_base) >> 32) & 0xffffffff));
    elog (DEBUG2, " Write ACTION_PKT_INIT_ADDR done! \n");

    action_write (h, ACTION_PATT_CARD_DDR_ADDR_L, 0);
    action_write (h, ACTION_PATT_CARD_DDR_ADDR_H, 0);
    elog (DEBUG2, " Write ACTION_PATT_CARD_DDR_ADDR done! \n");

    action_write (h, ACTION_STAT_INIT_ADDR_L,
                  (uint32_t) (((uint64_t) stat_dest_base) & 0xffffffff));
    action_write (h, ACTION_STAT_INIT_ADDR_H,
                  (uint32_t) ((((uint64_t) stat_dest_base) >> 32) & 0xffffffff));
    elog (DEBUG2, " Write ACTION_STAT_INIT_ADDR done! \n");

    action_write (h, ACTION_PATT_TOTAL_NUM_L,
                  (uint32_t) (((uint64_t) patt_size) & 0xffffffff));
    action_write (h, ACTION_PATT_TOTAL_NUM_H,
                  (uint32_t) ((((uint64_t) patt_size) >> 32) & 0xffffffff));
    elog (DEBUG2, " Write ACTION_PATT_TOTAL_NUM done! \n");

    action_write (h, ACTION_PKT_TOTAL_NUM_L,
                  (uint32_t) (((uint64_t) pkt_size) & 0xffffffff));
    action_write (h, ACTION_PKT_TOTAL_NUM_H,
                  (uint32_t) ((((uint64_t) pkt_size) >> 32) & 0xffffffff));
    elog (DEBUG2, " Write ACTION_PKT_TOTAL_NUM done! \n");

    action_write (h, ACTION_STAT_TOTAL_SIZE_L,
                  (uint32_t) (((uint64_t) stat_size) & 0xffffffff));
    action_write (h, ACTION_STAT_TOTAL_SIZE_H,
                  (uint32_t) ((((uint64_t) stat_size) >> 32) & 0xffffffff));
    elog (DEBUG2, " Write ACTION_STAT_TOTAL_SIZE done! \n");

    // Start copying the pattern from host memory to card
    action_write (h, ACTION_CONTROL_L, 0x00000001);
    action_write (h, ACTION_CONTROL_H, 0x00000000);
    elog (DEBUG2, " Write ACTION_CONTROL for pattern copying! \n");

    print_control_status (h);

    do {
        reg_data = action_read (h, ACTION_STATUS_L);
        elog (DEBUG3, "Pattern Phase: polling Status reg with 0X%X\n", reg_data);

        // Status[23:8]
        if ((reg_data & 0x00FFFF00) != 0) {
            elog (DEBUG1, "Error code got 0X%X\n", ((reg_data & 0x00FFFF00) >> 8));
            exit (EXIT_FAILURE);
        }

        // Status[0]
        if ((reg_data & 0x00000001) == 1) {
            elog (DEBUG1, "Pattern copy done!\n");
            break;
        }
    } while (1);

    //start_time = get_usec();
    // Start working control[2:1] = 11
    action_write (h, ACTION_CONTROL_L, 0x00000006);
    action_write (h, ACTION_CONTROL_H, 0x00000000);
    elog (DEBUG1, " Write ACTION_CONTROL for working! \n");

    do {
        reg_data = action_read (h, ACTION_STATUS_L);
        elog (DEBUG1, "Packet Phase: polling Status reg with 0X%X\n", reg_data);

        // Status[23:8]
        if ((reg_data & 0x00FFFF00) != 0) {
            elog (DEBUG1, "Error code got 0X%X\n", ((reg_data & 0x00FFFF00) >> 8));
            exit (EXIT_FAILURE);
        }

        // Status[0]
        if ((reg_data & 0x00000010) != 0) {
            elog (DEBUG1, "Memory space for stat used up!\n");
            exit (EXIT_FAILURE);
        }

        if ((reg_data & 0x00000006) == 6) {
            elog (DEBUG1, "Work done!\n");

            //reg_data = action_read(h, ACTION_STATUS_H);
            //elog (DEBUG1,"%d bytes of valid stat data transfered!\n", reg_data);

            break;
        }

        //// TODO: for test
        //if ((reg_data & 0x00000080) == 0x80) {
        //    elog (DEBUG1,"Run out!\n");

        //    //reg_data = action_read(h, ACTION_STATUS_H);
        //    //elog (DEBUG1,"%d bytes of valid stat data transfered!\n", reg_data);

        //    break;
        //}

        //reg_data = action_read(h, ACTION_LOG_L);
        //elog (DEBUG1,"Packet Phase: LOG_l reg 0X%X\n", reg_data);
        //reg_data = action_read(h, ACTION_LOG_H);
        //elog (DEBUG1,"Packet Phase: LOG_h reg 0X%X\n", reg_data);

    } while (1);

    //elapsed_time = get_usec() - start_time;

    //print_time(elapsed_time, pkt_size);

    //// TODO: for test
    //usleep(1000000);

    // Stop working
    action_write (h, ACTION_CONTROL_L, 0x00000000);
    action_write (h, ACTION_CONTROL_H, 0x00000000);
    elog (DEBUG2, " Write ACTION_CONTROL for stop working! \n");

    // Flush rest data
    action_write (h, ACTION_CONTROL_L, 0x00000008);
    action_write (h, ACTION_CONTROL_H, 0x00000000);
    elog (DEBUG2, " Write ACTION_CONTROL for stat flushing! \n");

    do {
        reg_data = action_read (h, ACTION_STATUS_L);

        // Status[23:8]
        if ((reg_data & 0x00FFFF00) != 0) {
            elog (DEBUG1, "Error code got 0X%X\n", ((reg_data & 0x00FFFF00) >> 8));
            exit (EXIT_FAILURE);
        }

        // Status[3]
        if ((reg_data & 0x00000008) == 8) {
            elog (DEBUG2, "Stat flush done!\n");
            reg_data = action_read (h, ACTION_STATUS_H);
            elog (DEBUG1, "Number of matched packets: %d\n", reg_data);
            *num_matched_pkt = reg_data;
            break;
        }

        elog (DEBUG3, "Polling Status reg with 0X%X\n", reg_data);
    } while (1);

    // Stop flushing
    action_write (h, ACTION_CONTROL_L, 0x00000000);
    action_write (h, ACTION_CONTROL_H, 0x00000000);
    elog (DEBUG2, " Write ACTION_CONTROL for stop working! \n");

    return;
}

static int sm_scan (struct snap_card* dnc,
                    int timeout,
                    void* patt_src_base,
                    void* pkt_src_base,
                    void* stat_dest_base,
                    size_t* num_matched_pkt,
                    size_t patt_size,
                    size_t pkt_size,
                    size_t stat_size)
{
    int rc;
    uint64_t td;

    rc = 0;

    action_sm (dnc, patt_src_base, pkt_src_base, stat_dest_base, num_matched_pkt,
               patt_size, pkt_size, stat_size);
    elog (DEBUG3, "Wait for idle\n");
    rc = action_wait_idle (dnc, timeout, &td);
    elog (DEBUG3, "Card in idle\n");

    if (0 != rc) {
        return rc;
    }

    return rc;
}

static struct snap_action* get_action (struct snap_card* handle,
                                       snap_action_flag_t flags, int timeout)
{
    struct snap_action* act;

    act = snap_attach_action (handle, ACTION_TYPE_STRING_MATCH,
                              flags, timeout);

    if (NULL == act) {
        elog (DEBUG1, "Error: Can not attach Action: %x\n", ACTION_TYPE_STRING_MATCH);
        elog (DEBUG1, "       Try to run snap_main tool\n");
    }

    return act;
}

static void* sm_compile (char* patt, size_t* size)
{
    // The max size that should be alloc
    // Assume we have at most 1024 lines in a pattern file
    size_t max_alloc_size = 1024 * (64 +
                                    (PATTERN_WIDTH_BYTES - 4) +
                                    ((PATTERN_WIDTH_BYTES - 4) % 64) == 0 ? 0 :
                                    (64 - ((PATTERN_WIDTH_BYTES - 4) % 64)));

    void* patt_src_base = alloc_mem (64, max_alloc_size);
    //void* patt_src_base = palloc0 (max_alloc_size);
    void* patt_src = patt_src_base;

    elog (DEBUG1, "PATTERN Source Address Start at 0X%016lX\n", (uint64_t)patt_src);

    if (patt == NULL) {
        elog (DEBUG1, "PATTERN pointer is NULL!\n");
        exit (EXIT_FAILURE);
    }

    //remove_newline (patt);
    // TODO: fill the same pattern for 8 times, workaround for 32x8.
    for (int i = 0; i < 8; i++) {
        elog (DEBUG3, "%s\n", patt);
        patt_src = fill_one_pattern (patt, patt_src);
        elog (DEBUG3, "Pattern Source Address 0X%016lX\n", (uint64_t)patt_src);
    }

    elog (DEBUG1, "Total size of pattern buffer used: %ld\n", (uint64_t) (patt_src - patt_src_base));

    elog (DEBUG1, "---------- Pattern Buffer: %p\n", patt_src_base);

    if (verbose_level > 2) {
        __hexdump (stdout, patt_src_base, (patt_src - patt_src_base));
    }

    (*size) = patt_src - patt_src_base;

    return patt_src_base;
}

static void* sm_pkt_psql (WindowObject* win, int row_count, size_t* size, size_t* size_for_sw)
{
    char* line = NULL;
    ssize_t read;
    bool isnull = true, isout = false;

    // The max size that should be alloc
    size_t max_alloc_size = MAX_NUM_PKT * (64 + 2048);

    void* pkt_src_base = alloc_mem (64, max_alloc_size);
    //void* pkt_src_base = palloc0 (max_alloc_size);
    void* pkt_src = pkt_src_base;

    elog (DEBUG1, "PACKET Source Address Start at 0X%016lX\n", (uint64_t)pkt_src);

    for (int i = 0; i < row_count; i++) {
        line = TextDatumGetCString (
                   WinGetFuncArgInPartition (*win, 0, i,
                                             WINDOW_SEEK_HEAD, false, &isnull, &isout));

        if (isnull) {
            elog (DEBUG1, "PACKET pointer is NULL!\n");
            exit (EXIT_FAILURE);
        }

        read = strlen (line);
        elog (DEBUG3, "PACKET line read with length %zu :\n", read);
        elog (DEBUG3, "%s\n", line);
        (*size_for_sw) += read;
        pkt_src = fill_one_packet (line, read, pkt_src);
        elog (DEBUG3, "PACKET Source Address 0X%016lX\n", (uint64_t)pkt_src);
    }

    elog (DEBUG1, "Total size of packet buffer used: %ld\n", (uint64_t) (pkt_src - pkt_src_base));

    elog (DEBUG1, "---------- Packet Buffer: %p\n", pkt_src_base);

    if (verbose_level > 2) {
        __hexdump (stdout, pkt_src_base, (pkt_src - pkt_src_base));
    }

    (*size) = pkt_src - pkt_src_base;

    return pkt_src_base;
}

static int get_results (void* result, size_t num_matched_pkt, void* stat_dest_base)
{
    int i = 0, j = 0;
    //uint16_t offset = 0;
    uint32_t pkt_id = 0;
    //uint32_t patt_id = 0;

    if (result == NULL) {
        elog (DEBUG1, "Invalid result pointer.\n");
        return 1;
    }

    for (i = 0; i < (int) ((OUTPUT_STAT_WIDTH / 8) * num_matched_pkt); i++) {
        elog (DEBUG2, "OUTPUT[%d] %#X\n", i, ((uint8_t*)stat_dest_base)[i]);
    }

    elog (DEBUG1, "---- Results (HW: hardware) ----\n");
    elog (DEBUG1, "PKT(HW) PATT(HW) OFFSET(HW)\n");

    for (i = 0; i < (int)num_matched_pkt; i++) {
        //for (j = 0; j < 4; j++) {
        //    patt_id |= (((uint8_t *)stat_dest_base)[i * 10 + j] << j * 8);
        //}

        for (j = 4; j < 8; j++) {
            pkt_id |= (((uint8_t*)stat_dest_base)[i * 10 + j] << (j % 4) * 8);
        }

        elog (DEBUG1, "MATCHED PKT: %d\n", pkt_id);
        ((uint8_t*)result)[pkt_id - 1] = 1;

        //for (j = 8; j < 10; j++) {
        //    offset |= (((uint8_t *)stat_dest_base)[i * 10 + j] << (j % 2) * 8);
        //}

        //patt_id = 0;
        pkt_id = 0;
        //offset = 0;
    }

    return 0;
}


static Datum
regex_capi (PG_FUNCTION_ARGS)
{
    char device[64];
    struct snap_card* dn;   /* lib snap handle */
    int card_no = 0;
    int rc = 0;
    //uint64_t cir;
    int timeout = ACTION_WAIT_TIME;
    //int no_chk_offset = 0;
    snap_action_flag_t attach_flags = 0;
    struct snap_action* act = NULL;
    //unsigned long ioctl_data;
    void* patt_src_base = NULL;
    void* pkt_src_base = NULL;
    void* stat_dest_base = NULL;
    size_t num_matched_pkt = 0;
    size_t pkt_size = 0;
    size_t patt_size = 0;
    size_t pkt_size_for_sw = 0;
    uint64_t hw_start_time;
    uint64_t hw_elapsed_time;
    uint64_t patt_start_time;
    uint64_t patt_elapsed_time;
    uint64_t pkt_start_time;
    uint64_t pkt_elapsed_time;
    uint64_t result_start_time;
    uint64_t result_elapsed_time;
    uint64_t start_time;
    uint64_t elapsed_time;
    uint64_t pre_start_time;
    uint64_t pre_elapsed_time;
    uint64_t pre_db_start_time;
    uint64_t pre_db_elapsed_time;
    uint64_t post_start_time;
    uint64_t post_elapsed_time;
    uint32_t reg_data;
    int count = 0;
    // Alloc state output buffer, aligned to 4K
    //int real_stat_size = (OUTPUT_STAT_WIDTH / 8) * regex_ref_get_num_matched_pkt();
    int real_stat_size = 0;
    int stat_size = 0;

    int N;
    char* cstr_p = NULL;
    bool isnull = true;
    PATTERN_ID = 0;
    PACKET_ID = 0;

    //snap_card_ioctl (dn, GET_SDRAM_SIZE, (unsigned long)&ioctl_data);
    //elog (DEBUG1," Card, %d MB of Card Ram avilable.\n", (int)ioctl_data);

    WindowObject winobj = PG_WINDOW_OBJECT();
    psql_regex_capi_context* context;
    int64 curpos;

    context = (psql_regex_capi_context*)
              WinGetPartitionLocalMemory (winobj,
                                          sizeof (psql_regex_capi_context));

    elog (DEBUG1, "In regex_capi\n");

    if (!context->isdone) {
        start_time = get_usec();
        pre_start_time = get_usec();

        elog (DEBUG2, "Open Card: %d\n", card_no);
        sprintf (device, "/dev/cxl/afu%d.0s", card_no);
        dn = snap_card_alloc_dev (device, SNAP_VENDOR_ID_IBM, SNAP_DEVICE_ID_SNAP);

        if (NULL == dn) {
            errno = ENODEV;
            elog (DEBUG1, "ERROR: snap_card_alloc_dev(%s)\n", device);
            return -1;
        }

        // Reset the hardware
        soft_reset (dn);

        elog (DEBUG1, "Start to get action.\n");
        act = get_action (dn, attach_flags, 5 * timeout);
        elog (DEBUG1, "Finish get action.\n");

        pre_elapsed_time = get_usec() - pre_start_time;
        elog (LOG, "Card prepare time:\n");
        print_time (pre_elapsed_time, 1);

        pre_db_start_time = get_usec();
        N = (int) WinGetPartitionRowCount (winobj);
        pre_db_elapsed_time = get_usec() - pre_db_start_time;
        elog (LOG, "DB prepare time:\n");
        print_time (pre_db_elapsed_time, 1);

        real_stat_size = (OUTPUT_STAT_WIDTH / 8) * N;
        stat_size = (real_stat_size % 4096 == 0) ? real_stat_size : real_stat_size + (4096 - (real_stat_size % 4096));

        // At least 4K for output buffer.
        if (stat_size == 0) {
            stat_size = 4096;
        }

        context->result = palloc0 (N);
        stat_dest_base = alloc_mem (64, stat_size);
        //memset (stat_dest_base, 0, stat_size);

        elog (DEBUG1, "======== COMPILE PATTERN FILE ========\n");
        // Compile the regular expression
        patt_start_time = get_usec();

        cstr_p = TextDatumGetCString (
                     WinGetFuncArgCurrent (winobj, 1, &isnull));

        if (!isnull) {
            elog (DEBUG1, "pattern: %s\n", cstr_p);
        }

        patt_src_base = sm_compile (cstr_p, &patt_size);
        patt_elapsed_time = get_usec() - patt_start_time;
        elog (LOG, "Pattern compile time:\n");
        print_time (patt_elapsed_time, patt_size);
        elog (DEBUG1, "Pattern buffer size: %zu\n", patt_size);
        elog (DEBUG1, "======== COMPILE PATTERN FILE DONE ========\n");

        elog (DEBUG1, "======== COMPILE PACKET FILE ========\n");
        // Compile the packets
        pkt_start_time = get_usec();
        pkt_src_base = sm_pkt_psql (&winobj, N, &pkt_size, &pkt_size_for_sw);
        pkt_elapsed_time = get_usec() - pkt_start_time;
        elog (LOG, "Packet compile time:\n");
        print_time (pkt_elapsed_time, pkt_size);
        elog (DEBUG1, "======== COMPILE PACKET FILE DONE ========\n");

        elog (DEBUG1, "======== HARDWARE RUN ========\n");
        hw_start_time = get_usec();
        rc = sm_scan (dn, timeout,
                      patt_src_base,
                      pkt_src_base,
                      stat_dest_base,
                      &num_matched_pkt,
                      patt_size,
                      pkt_size,
                      stat_size);
        hw_elapsed_time = get_usec() - hw_start_time;
        // pkt_size_for_sw is the real size without hardware specific 64B header
        elog (LOG, "HW run time:\n");
        print_time (hw_elapsed_time, pkt_size_for_sw);

        elog (DEBUG1, "Finish sm_scan with %d matched packets.\n", (int)num_matched_pkt);
        elog (DEBUG1, "======== HARDWARE DONE========\n");

        result_start_time = get_usec();

        // Wait for transaction to be done.
        do {
            //elog (DEBUG3, " Draining %i! \n", count);
            action_read (dn, ACTION_STATUS_L);
            count++;
        } while (count < 2);

        reg_data = action_read (dn, ACTION_STATUS_H);
        elog (DEBUG1, "After draining, number of matched packets: %d\n", reg_data);
        num_matched_pkt = reg_data;

        if (get_results (context->result, num_matched_pkt, stat_dest_base)) {
            errno = ENODEV;
            elog (DEBUG1, "ERROR: failed to get results.\n");
            return -1;
        }

        result_elapsed_time = get_usec() - result_start_time;
        elog (LOG, "Result harvest time:\n");
        print_time (result_elapsed_time, stat_size);

        post_start_time = get_usec();
        snap_detach_action (act);
        // Unmap AFU MMIO registers, if previously mapped
        snap_card_free (dn);
        elog (DEBUG2, "Free Card Handle: %p\n", dn);

        free_mem (patt_src_base);
        free_mem (pkt_src_base);
        free_mem (stat_dest_base);
        pfree (context->result);

        context->isdone = true;

        post_elapsed_time = get_usec() - post_start_time;
        elog (LOG, "Post function cleanup time:\n");
        print_time (post_elapsed_time, stat_size);

        elapsed_time = get_usec() - start_time;
        // pkt_size_for_sw is the real size without hardware specific 64B header
        elog (LOG, "Total time:\n");
        print_time (elapsed_time, pkt_size_for_sw);
        elog (DEBUG1, "End of Test rc: %d\n", rc);
    }

    if (context->isnull) {
        PG_RETURN_NULL();
    }

    curpos = WinGetCurrentPosition (winobj);
    PG_RETURN_INT32 ((int) (context->result[curpos]));
}

Datum
psql_regex_capi (PG_FUNCTION_ARGS)
{
    PG_RETURN_DATUM (regex_capi (fcinfo));
}


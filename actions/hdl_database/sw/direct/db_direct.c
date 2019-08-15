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


#include "db_direct.h"
#include "utils/fregex.h"
#include "regex_ref.h"
#include "Interface.h"

#define VERBOSE0(fmt, ...) do {         \
        printf(fmt, ## __VA_ARGS__);    \
    } while (0)

#define VERBOSE1(fmt, ...) do {         \
        if (verbose_level > 0)          \
            printf(fmt, ## __VA_ARGS__);    \
    } while (0)

#define VERBOSE2(fmt, ...) do {         \
        if (verbose_level > 1)          \
            printf(fmt, ## __VA_ARGS__);    \
    } while (0)


#define VERBOSE3(fmt, ...) do {         \
        if (verbose_level > 2)          \
            printf(fmt, ## __VA_ARGS__);    \
    } while (0)

#define VERBOSE4(fmt, ...) do {         \
        if (verbose_level > 3)          \
            printf(fmt, ## __VA_ARGS__);    \
    } while (0)

static uint32_t PATTERN_ID = 0;
//static uint32_t PACKET_ID = 0;
//static const char* version = GIT_VERSION;
static  int verbose_level = 0;

void print_error (const char* file, const char* func, const char* line, int rc)
{
    printf ("ERROR: %s %s failed in line %s with return code %d\n", file, func, line, rc);
}

int64_t diff_time (struct timespec* t_beg, struct timespec* t_end)
{
    if (t_end == NULL || t_beg == NULL) {
        return 0;
    }

    return ((t_end-> tv_sec - t_beg-> tv_sec) * 1000000000L + t_end-> tv_nsec - t_beg-> tv_nsec);
}

uint64_t get_usec (void)
{
    struct timeval t;

    gettimeofday (&t, NULL);
    return t.tv_sec * 1000000 + t.tv_usec;
}

int get_file_line_count (FILE* fp)
{
    int lines = 0;
    char ch;

    while (!feof (fp)) {
        ch = fgetc (fp);

        if (ch == '\n') {
            lines++;
        }
    }

    return lines;
}

void remove_newline (char* str)
{
    char* pos;

    if ((pos = strchr (str, '\n')) != NULL) {
        *pos = '\0';
    } else {
        VERBOSE0 ("Input too long for remove_newline ... ");
        exit (EXIT_FAILURE);
    }
}

// to be replaced by perf_calc in pg_capi_internal.c
float print_time (uint64_t elapsed, uint64_t size)
{
    int t;
    float fsize = (float)size / (1024 * 1024);
    float ft;

    if (elapsed > 10000) {
        t = (int)elapsed / 1000;
        ft = (1000 / (float)t) * fsize;
        //VERBOSE0 (" end after %d msec (%0.3f MB/sec)\n", t, ft);
        //VERBOSE0 ("%d msec %0.3f\n", t, ft);
    } else {
        t = (int)elapsed;
        ft = (1000000 / (float)t) * fsize;
        //VERBOSE0 (" end after %d usec (%0.3f MB/sec)\n", t, ft);
        //VERBOSE0 ("%d usec %0.3f\n", t, ft);
    }
    return ft;
}

// to be replaced in pg_capi_internal.c
void* alloc_mem (int align, size_t size)
{
    void* a;
    size_t size2 = size + align;

    VERBOSE2 ("%s Enter Align: %d Size: %zu\n", __func__, align, size);

    if (posix_memalign ((void**)&a, 4096, size2) != 0) {
        perror ("FAILED: posix_memalign()");
        return NULL;
    }

    VERBOSE2 ("%s Exit %p\n", __func__, a);
    return a;
}

// to be replaced in pg_capi_internal.c
void free_mem (void* a)
{
    VERBOSE2 ("Free Mem %p\n", a);

    if (a) {
        free (a);
    }
}

//static void memset2 (void* a, uint64_t pattern, int size)
//{
//    int i;
//    uint64_t* a64 = a;
//
//    for (i = 0; i < size; i += 8) {
//        *a64 = (pattern & 0xffffffff) | (~pattern << 32ull);
//        a64++;
//        pattern += 8;
//    }
//}

void* fill_one_packet (const char* in_pkt, int size, void* in_pkt_addr, int in_pkt_id)
{
    unsigned char* pkt_base_addr = in_pkt_addr;
    int pkt_id;
    uint32_t bytes_used = 0;
    uint16_t pkt_len = size;

     // The TAG ID
    pkt_id = in_pkt_id;

    VERBOSE2 ("PKT[%d] %s len %d\n", pkt_id, in_pkt, pkt_len);

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

void* fill_one_pattern (const char* in_patt, void* in_patt_addr)
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

    VERBOSE1 ("PATT[%d] %s\n", pattern_id, in_patt);

    fregex_get_config (in_patt,
                       MAX_TOKEN_NUM,
                       MAX_STATE_NUM,
                       MAX_CHAR_NUM,
                       MAX_CHAR_PER_TOKEN,
                       config_bytes,
                       &config_len,
                       0);

    VERBOSE2 ("Config length (bits)  %d\n", config_len * 8);
    VERBOSE2 ("Config length (bytes) %d\n", config_len);

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

// to be replaced by action_write in pg_capi_internal.c
/* Action or Kernel Write and Read are 32 bit MMIO */
void action_write (struct snap_card* h, uint32_t addr, uint32_t data)
{
    int rc;

    rc = snap_mmio_write32 (h, (uint64_t)addr, data);

    if (0 != rc) {
        VERBOSE0 ("Write MMIO 32 Err\n");
    }

    return;
}

// to be replaced by aciton_write in pg_capi_internal.c
uint32_t action_read (struct snap_card* h, uint32_t addr)
{
    int rc;
    uint32_t data;

    rc = snap_mmio_read32 (h, (uint64_t)addr, &data);

    if (0 != rc) {
        VERBOSE0 ("Read MMIO 32 Err\n");
    }

    return data;
}

/*  Calculate msec to FPGA ticks.
 *  we run at 250 Mhz on FPGA so 4 ns per tick
 */
//static uint32_t msec_2_ticks (int msec)
//{
//    uint32_t fpga_ticks = msec;
//
//    fpga_ticks = fpga_ticks * 250;
//#ifndef _SIM_
//    fpga_ticks = fpga_ticks * 1000;
//#endif
//    VERBOSE1 (" fpga Ticks = %d (0x%x)", fpga_ticks, fpga_ticks);
//    return fpga_ticks;
//}


// to be replaced in pg_capi_internal.c
/*
 *  Start Action and wait for Idle.
 */
int action_wait_idle (struct snap_card* h, int timeout, uint64_t* elapsed)
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
        VERBOSE0 ("Error. Timeout while Waiting for Idle\n");
    }

    *elapsed = td;
    return rc;
}

// to be replaced in pg_capi_internal.c
void print_control_status (struct snap_card* h, int eng_id)
{
    if (verbose_level > 2) {
        uint32_t reg_data;
        VERBOSE3 (" READ Control and Status Registers: \n");
        reg_data = action_read (h, REG(ACTION_STATUS_L, eng_id));
        VERBOSE3 ("       STATUS_L = 0x%x\n", reg_data);
        reg_data = action_read (h, REG(ACTION_STATUS_H, eng_id));
        VERBOSE3 ("       STATUS_H = 0x%x\n", reg_data);
        reg_data = action_read (h, REG(ACTION_CONTROL_L, eng_id));
        VERBOSE3 ("       CONTROL_L = 0x%x\n", reg_data);
        reg_data = action_read (h, REG(ACTION_CONTROL_H, eng_id));
        VERBOSE3 ("       CONTROL_H = 0x%x\n", reg_data);
    }
}

// to be replaced in pg_capi_internal.c
void soft_reset (struct snap_card* h, int eng_id)
{
    // Status[4] to reset
    action_write (h, REG(ACTION_CONTROL_L, eng_id), 0x00000010);
    action_write (h, REG(ACTION_CONTROL_H, eng_id), 0x00000000);
    VERBOSE2 (" Write ACTION_CONTROL for soft reset! \n");
    action_write (h, REG(ACTION_CONTROL_L, eng_id), 0x00000000);
    action_write (h, REG(ACTION_CONTROL_H, eng_id), 0x00000000);
}

// to be replaced in pg_capi_internal.c
void action_regex (struct snap_card* h,
                       void* patt_src_base,
                       void* pkt_src_base,
                       void* stat_dest_base,
                       size_t* num_matched_pkt,
                       size_t patt_size,
                       size_t pkt_size,
                       size_t stat_size,
                       int eng_id)
{
    uint32_t reg_data;
    //uint64_t start_time;
    //uint64_t elapsed_time;

    VERBOSE2 (" ------ Regular Expression Start -------- \n");
    VERBOSE2 (" PATTERN SOURCE ADDR: %p -- SIZE: %d\n", patt_src_base, (int)patt_size);
    VERBOSE2 (" PACKET  SOURCE ADDR: %p -- SIZE: %d\n", pkt_src_base, (int)pkt_size);
    VERBOSE2 (" STAT    DEST   ADDR: %p -- SIZE(max): %d\n", stat_dest_base, (int)stat_size);

    VERBOSE2 (" Start register config! \n");
    print_control_status (h, eng_id);

    action_write (h, REG(ACTION_PATT_INIT_ADDR_L, eng_id),
                  (uint32_t) (((uint64_t) patt_src_base) & 0xffffffff));
    action_write (h, REG(ACTION_PATT_INIT_ADDR_H, eng_id),
                  (uint32_t) ((((uint64_t) patt_src_base) >> 32) & 0xffffffff));
    VERBOSE2 (" Write ACTION_PATT_INIT_ADDR done! \n");

    action_write (h, REG(ACTION_PKT_INIT_ADDR_L, eng_id),
                  (uint32_t) (((uint64_t) pkt_src_base) & 0xffffffff));
    action_write (h, REG(ACTION_PKT_INIT_ADDR_H, eng_id),
                  (uint32_t) ((((uint64_t) pkt_src_base) >> 32) & 0xffffffff));
    VERBOSE2 (" Write ACTION_PKT_INIT_ADDR done! \n");

    action_write (h, REG(ACTION_PATT_CARD_DDR_ADDR_L, eng_id), 0);
    action_write (h, REG(ACTION_PATT_CARD_DDR_ADDR_H, eng_id), 0);
    VERBOSE2 (" Write ACTION_PATT_CARD_DDR_ADDR done! \n");

    action_write (h, REG(ACTION_STAT_INIT_ADDR_L, eng_id),
                  (uint32_t) (((uint64_t) stat_dest_base) & 0xffffffff));
    action_write (h, REG(ACTION_STAT_INIT_ADDR_H, eng_id),
                  (uint32_t) ((((uint64_t) stat_dest_base) >> 32) & 0xffffffff));
    VERBOSE2 (" Write ACTION_STAT_INIT_ADDR done! \n");

    action_write (h, REG(ACTION_PATT_TOTAL_NUM_L, eng_id),
                  (uint32_t) (((uint64_t) patt_size) & 0xffffffff));
    action_write (h, REG(ACTION_PATT_TOTAL_NUM_H, eng_id),
                  (uint32_t) ((((uint64_t) patt_size) >> 32) & 0xffffffff));
    VERBOSE2 (" Write ACTION_PATT_TOTAL_NUM done! \n");

    action_write (h, REG(ACTION_PKT_TOTAL_NUM_L, eng_id),
                  (uint32_t) (((uint64_t) pkt_size) & 0xffffffff));
    action_write (h, REG(ACTION_PKT_TOTAL_NUM_H, eng_id),
                  (uint32_t) ((((uint64_t) pkt_size) >> 32) & 0xffffffff));
    VERBOSE2 (" Write ACTION_PKT_TOTAL_NUM done! \n");

    action_write (h, REG(ACTION_STAT_TOTAL_SIZE_L, eng_id),
                  (uint32_t) (((uint64_t) stat_size) & 0xffffffff));
    action_write (h, REG(ACTION_STAT_TOTAL_SIZE_H, eng_id),
                  (uint32_t) ((((uint64_t) stat_size) >> 32) & 0xffffffff));
    VERBOSE2 (" Write ACTION_STAT_TOTAL_SIZE done! \n");

    // Start copying the pattern from host memory to card
    action_write (h, REG(ACTION_CONTROL_L, eng_id), 0x00000001);
    action_write (h, REG(ACTION_CONTROL_H, eng_id), 0x00000000);
    VERBOSE2 (" Write ACTION_CONTROL for pattern copying! \n");

    print_control_status (h, eng_id);

    do {
        reg_data = action_read (h, REG(ACTION_STATUS_L, eng_id));
        VERBOSE3 ("Pattern Phase: polling Status reg with 0X%X\n", reg_data);

        // Status[23:8]
        if ((reg_data & 0x00FFFF00) != 0) {
            VERBOSE0 ("Error code got 0X%X\n", ((reg_data & 0x00FFFF00) >> 8));
            exit (EXIT_FAILURE);
        }

        // Status[0]
        if ((reg_data & 0x00000001) == 1) {
            VERBOSE1 ("Pattern copy done!\n");
            break;
        }
    } while (1);

    //start_time = get_usec();
    // Start working control[2:1] = 11
    action_write (h, REG(ACTION_CONTROL_L, eng_id), 0x00000006);
    action_write (h, REG(ACTION_CONTROL_H, eng_id), 0x00000000);
    VERBOSE1 (" Write ACTION_CONTROL for working! \n");

    do {
        reg_data = action_read (h, REG(ACTION_STATUS_L, eng_id));
        VERBOSE3 ("Packet Phase: polling Status reg with 0X%X\n", reg_data);

        // Status[23:8]
        if ((reg_data & 0x00FFFF00) != 0) {
            VERBOSE0 ("Error code got 0X%X\n", ((reg_data & 0x00FFFF00) >> 8));
            exit (EXIT_FAILURE);
        }

        // Status[0]
        if ((reg_data & 0x00000010) != 0) {
            VERBOSE0 ("Memory space for stat used up!\n");
            exit (EXIT_FAILURE);
        }

        if ((reg_data & 0x00000006) == 6) {
            VERBOSE1 ("Work done!\n");

            //reg_data = action_read(h, ACTION_STATUS_H);
            //VERBOSE1 ("%d bytes of valid stat data transfered!\n", reg_data);

            break;
        }

        //// TODO: for test
        //if ((reg_data & 0x00000080) == 0x80) {
        //    VERBOSE1 ("Run out!\n");

        //    //reg_data = action_read(h, ACTION_STATUS_H);
        //    //VERBOSE1 ("%d bytes of valid stat data transfered!\n", reg_data);

        //    break;
        //}

        //reg_data = action_read(h, ACTION_DEBUG0_L);
        //VERBOSE1("Packet Phase: debug0_l reg 0X%X\n", reg_data);
        //reg_data = action_read(h, ACTION_DEBUG0_H);
        //VERBOSE1("Packet Phase: debug0_h reg 0X%X\n", reg_data);

    } while (1);

    //elapsed_time = get_usec() - start_time;

    //print_time(elapsed_time, pkt_size);

    //// TODO: for test
    //usleep(1000000);

    // Stop working
    action_write (h, REG(ACTION_CONTROL_L, eng_id), 0x00000000);
    action_write (h, REG(ACTION_CONTROL_H, eng_id), 0x00000000);
    VERBOSE2 (" Write ACTION_CONTROL for stop working! \n");

    // Flush rest data
    action_write (h, REG(ACTION_CONTROL_L, eng_id), 0x00000008);
    action_write (h, REG(ACTION_CONTROL_H, eng_id), 0x00000000);
    VERBOSE2 (" Write ACTION_CONTROL for stat flushing! \n");

    do {
        reg_data = action_read (h, REG(ACTION_STATUS_L, eng_id));

        // Status[23:8]
        if ((reg_data & 0x00FFFF00) != 0) {
            VERBOSE0 ("Error code got 0X%X\n", ((reg_data & 0x00FFFF00) >> 8));
            exit (EXIT_FAILURE);
        }

        // Status[3]
        if ((reg_data & 0x00000008) == 8) {
            VERBOSE2 ("Stat flush done!\n");
            reg_data = action_read (h, REG(ACTION_STATUS_H, eng_id));
            VERBOSE1 ("Number of matched packets: %d\n", reg_data);
            *num_matched_pkt = reg_data;
            break;
        }

        VERBOSE3 ("Polling Status reg with 0X%X\n", reg_data);
    } while (1);

    // Stop flushing
    action_write (h, REG(ACTION_CONTROL_L, eng_id), 0x00000000);
    action_write (h, REG(ACTION_CONTROL_H, eng_id), 0x00000000);
    VERBOSE2 (" Write ACTION_CONTROL for stop working! \n");

    return;
}

// to be replaced by regex_scan_internal in pg_capi_internal.c
int regex_scan (struct snap_card* dnc,
                    int timeout,
                    void* patt_src_base,
                    void* pkt_src_base,
                    void* stat_dest_base,
                    size_t* num_matched_pkt,
                    size_t patt_size,
                    size_t pkt_size,
                    size_t stat_size,
                    int eng_id)
{
    int rc;
    uint64_t td;

    rc = 0;

    action_regex (dnc, patt_src_base, pkt_src_base, stat_dest_base, num_matched_pkt,
               patt_size, pkt_size, stat_size, eng_id);
    VERBOSE3 ("Wait for idle\n");
    rc = action_wait_idle (dnc, timeout, &td);
    VERBOSE3 ("Card in idle\n");

    if (0 != rc) {
        return rc;
    }

    return rc;
}

// to be replaced in pg_capi_internal.c
struct snap_action* get_action (struct snap_card* handle,
                                       snap_action_flag_t flags, int timeout)
{
    struct snap_action* act;

    act = snap_attach_action (handle, ACTION_TYPE_DATABASE,
                              flags, timeout);

    if (NULL == act) {
        VERBOSE0 ("Error: Can not attach Action: %x\n", ACTION_TYPE_DATABASE);
        VERBOSE0 ("       Try to run snap_main tool\n");
    }

    return act;
}

void usage (const char* prog)
{
    VERBOSE0 ("CAPI Database Acceleration Direct Test.\n"
              "    Use Option -p and -q for pattern and packet\n"
              "    e.g. %s -p <packet file> -q <pattern file> [-vv] [-I]\n",
              prog);
    VERBOSE0 ("Usage: %s\n"
              "    -h, --help           print usage information\n"
              "    -v, --verbose        verbose mode\n"
              "    -C, --card <cardno>  use this card for operation\n"
             // "    -V, --version\n"
              "    -q, --quiet          quiece output\n"
              "    -t, --timeout        Timeout after N sec (default 1 sec)\n"
              "    -I, --irq            Enable Action Done Interrupt (default No Interrupts)\n"
              "    -p, --packet         Packet file for matching\n"
              "    -q, --pattern        Pattern file for matching\n"
	      "    -e, --num_eng        set number of engines to use\n"
	      "    -j, --num_job        set number of jobs per thread\n"
	      "    -r, --repeat         set number of repeat tests\n"
              , prog);
}

void* sm_compile_file (const char* file_path, size_t* size)
{
    FILE* fp;
    char* line = NULL;
    size_t len = 0;
    ssize_t read;

    // The max size that should be alloc
    // Assume we have at most 1024 lines in a pattern file
    size_t max_alloc_size = 1024 * (64 +
                                    (PATTERN_WIDTH_BYTES - 4) +
                                    ((PATTERN_WIDTH_BYTES - 4) % 64) == 0 ? 0 :
                                    (64 - ((PATTERN_WIDTH_BYTES - 4) % 64)));

    void* patt_src_base = alloc_mem (64, max_alloc_size);
    void* patt_src = patt_src_base;

    VERBOSE1 ("PATTERN Source Address Start at 0X%016lX\n", (uint64_t)patt_src);

    fp = fopen (file_path, "r");

    if (fp == NULL) {
        VERBOSE0 ("PATTERN file not existed %s\n", file_path);
        exit (EXIT_FAILURE);
    }

    while ((read = getline (&line, &len, fp)) != -1) {
        remove_newline (line);
        read--;
        VERBOSE3 ("Pattern line read with length %zu :\n", read);
        VERBOSE3 ("%s\n", line);
        patt_src = fill_one_pattern (line, patt_src);
        // regex ref model
        regex_ref_push_pattern (line);
        VERBOSE3 ("Pattern Source Address 0X%016lX\n", (uint64_t)patt_src);
    }

    VERBOSE1 ("Total size of pattern buffer used: %ld\n", (uint64_t) (patt_src - patt_src_base));

    VERBOSE1 ("---------- Pattern Buffer: %p\n", patt_src_base);

    if (verbose_level > 2) {
        __hexdump (stdout, patt_src_base, (patt_src - patt_src_base));
    }

    fclose (fp);

    if (line) {
        free (line);
    }

    (*size) = patt_src - patt_src_base;

    return patt_src_base;
}

void* regex_scan_file (const char* file_path, size_t* size, size_t* size_for_sw,
	               int num_jobs, void** job_pkt_src_bases, size_t* job_sizes, int* pkt_count)
{
    FILE* fp = fopen (file_path, "r");
    char* line = NULL;
    size_t len = 0;
    ssize_t read;
    int lines_read = 0;
    int curr_job_id = 0;

    // The max size that should be alloc
    //size_t max_alloc_size = MAX_NUM_PKT * (64 + 2048);
    *pkt_count = get_file_line_count (fp);
    size_t pkt_num = *pkt_count < 4096 ? 4096 : *pkt_count;
    size_t max_alloc_size = pkt_num * (64 + 2048);

    void* pkt_src_base = alloc_mem (64, max_alloc_size);
    void* pkt_src = pkt_src_base;

    VERBOSE1 ("PACKET Source Address Start at 0X%016lX\n", (uint64_t)pkt_src);

    fp = fopen (file_path, "r");

    if (fp == NULL) {
        VERBOSE0 ("PACKET file not existed %s\n", file_path);
        exit (EXIT_FAILURE);
    }

    for (int i = 0; i < num_jobs; i++) {
	job_pkt_src_bases[i] = NULL;
	job_sizes[i] = 0;
    }

    job_pkt_src_bases[0] = pkt_src_base;

    while ((read = getline (&line, &len, fp)) != -1) {
	if (curr_job_id != num_jobs - 1 &&
	    lines_read == (curr_job_id + 1) * (pkt_num / num_jobs)) {
	    job_sizes[curr_job_id] = (unsigned char*) pkt_src - (unsigned char*) job_pkt_src_bases[curr_job_id];
	    curr_job_id++;
	    job_pkt_src_bases[curr_job_id] = pkt_src;
	}

        remove_newline (line);
        read--;
        VERBOSE3 ("PACKET line read with length %zu :\n", read);
        VERBOSE3 ("%s\n", line);
        (*size_for_sw) += read;
        pkt_src = fill_one_packet (line, read, pkt_src, lines_read + 1);
        // regex ref model
        regex_ref_push_packet (line);
        VERBOSE3 ("PACKET Source Address 0X%016lX\n", (uint64_t)pkt_src);

	lines_read++;
    }

    job_sizes[curr_job_id] = (unsigned char*)pkt_src - (unsigned char*) job_pkt_src_bases[curr_job_id];

    VERBOSE1 ("Total size of packet buffer used: %ld\n", (uint64_t) (pkt_src - pkt_src_base));

    VERBOSE1 ("---------- Packet Buffer: %p\n", pkt_src_base);

    if (verbose_level > 2) {
        __hexdump (stdout, pkt_src_base, (pkt_src - pkt_src_base));
    }

    fclose (fp);

    if (line) {
        free (line);
    }

    (*size) = pkt_src - pkt_src_base;

    return pkt_src_base;
}

// to be replaced in pg_capi_internal.c
int print_results (size_t num_results, void* stat_dest_base)
{
    int i = 0, j = 0;
    uint16_t offset = 0;
    uint32_t pkt_id = 0;
    uint32_t patt_id = 0;
    int rc = 0;

    VERBOSE0 ("---- Result buffer address: %p ----\n", stat_dest_base);
    VERBOSE0 ("---- Results (HW: hardware) ----\n");
    VERBOSE0 ("PKT(HW) PATT(HW) OFFSET(HW)\n");

    for (i = 0; i < (int)num_results; i++) {
        for (j = 0; j < 4; j++) {
            patt_id |= (((uint8_t*)stat_dest_base)[i * 10 + j] << j * 8);
        }

        for (j = 4; j < 8; j++) {
            pkt_id |= (((uint8_t*)stat_dest_base)[i * 10 + j] << (j % 4) * 8);
        }

        for (j = 8; j < 10; j++) {
            offset |= (((uint8_t*)stat_dest_base)[i * 10 + j] << (j % 2) * 8);
        }

        VERBOSE0 ("%7d\t%6d\t%7d\n", pkt_id, patt_id, offset);

        patt_id = 0;
        pkt_id = 0;
        offset = 0;
    }

    return rc;
}

int compare_num_matched_pkt (size_t num_matched_pkt)
{
    int rc = 0;

    if ((int)num_matched_pkt != regex_ref_get_num_matched_pkt()) {
        VERBOSE0 ("ERROR! Num matched packets mismatch\n");
        VERBOSE0 ("EXPECTED: %d\n", regex_ref_get_num_matched_pkt());
        VERBOSE0 ("ACTUAL: %d\n", (int)num_matched_pkt);
        rc = 1;
    }

    return rc;
}

int compare_result_id (uint32_t result_id)
{
    int rc = 0;
    sm_stat ref_stat = regex_ref_get_result (result_id);

    if (ref_stat.packet_id != result_id) {
        VERBOSE0 ("PKT(HW): %7d\tPKT(SW): %7d", result_id, ref_stat.packet_id);
        VERBOSE0 (" MISMATCHED!\n");
        rc = 1;
    } else {
        VERBOSE1 ("PKT(HW): %7d\tPKT(SW): %7d", result_id, ref_stat.packet_id);
        VERBOSE1 ("    MATCHED!\n");
    }

    return rc;
}

int main (int argc, char* argv[])
{
    //printf("main start\n");
    char device[64];
    struct snap_card* dn;   /* lib snap handle */
    int card_no = 0;
    int cmd;
    //int rc = 1;
    uint64_t cir;
    int timeout = ACTION_WAIT_TIME;
    int num_eng_using = 1;
    int num_job_per_thd = 1;
    int num_repeat = 1;
    //int no_chk_offset = 0;
    snap_action_flag_t attach_flags = 0;
    struct snap_action* act = NULL;
    unsigned long ioctl_data;
    void* patt_src_base = NULL;
    void* pkt_src_base = NULL;
    //void* pkt_src_base_0 = NULL;
    //void* stat_dest_base_0 = NULL;
    //size_t num_matched_pkt = 0;
    size_t pkt_size = 0;
    size_t patt_size = 0;
    size_t pkt_size_for_sw = 0;
    void** job_pkt_src_bases;
    size_t* job_pkt_sizes;
    int pkt_count = 0;
    uint64_t start_time;
    uint64_t elapsed_time;
    //uint32_t reg_data;
    uint32_t hw_version = 0;
    int num_engines = 0;
    int num_pkt_pipes = 0;
    int num_patt_pipes = 0;
    int revision = 0;

    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            { "card",         required_argument, NULL, 'C' },
            { "verbose",      no_argument,       NULL, 'v' },
            { "help",         no_argument,       NULL, 'h' },
           // { "version",      no_argument,       NULL, 'V' },
            { "quiet",        no_argument,       NULL, 'q' },
            { "timeout",      required_argument, NULL, 't' },
            { "irq",          no_argument,       NULL, 'I' },
           // { "no_chk_offset", no_argument,       NULL, 'f' },
            { "packet",       required_argument, NULL, 'p' },
            { "pattern",      required_argument, NULL, 'q' },
	    { "num_eng",      required_argument, NULL, 'e' },
	    { "num_job",      required_argument, NULL, 'j' },
	    { "repeat",       required_argument, NULL, 'r' },
            { 0,              no_argument,       NULL, 0   },
        };
        cmd = getopt_long (argc, argv, "C:t:p:q:e:j:r:Iqvh",
                           long_options, &option_index);

        if (cmd == -1) {  /* all params processed ? */
            break;
        }

        switch (cmd) {
        case 'v':   /* verbose */
            verbose_level++;
            break;

        //case 'V':    version
           // VERBOSE0 ("%s\n", version);
           // exit (EXIT_SUCCESS);;

        case 'h':   /* help */
            usage (argv[0]);
            exit (EXIT_SUCCESS);;

        case 'C':   /* card */
            card_no = strtol (optarg, (char**)NULL, 0);
            break;

        case 't':
            timeout = strtol (optarg, (char**)NULL, 0);  /* in sec */
            break;

        case 'I':      /* irq */ 
            attach_flags = SNAP_ACTION_DONE_IRQ | SNAP_ATTACH_IRQ;
            break;

	case 'e':
	    num_eng_using = strtol (optarg, (char**)NULL, 0);
	    break;
	
	case 'j':
	    num_job_per_thd = strtol (optarg, (char**)NULL, 0);
	    break;

	case 'r':
	    num_repeat = strtol (optarg, (char**)NULL, 0);
	    break;

        //case 'f':       don't check offset 
          //  no_chk_offset = 1;
          //  break;

        default:
            usage (argv[0]);
            exit (EXIT_FAILURE);
        }
    }
    
    printf ("Open Card: %d\n", card_no);
    VERBOSE2 ("Open Card: %d\n", card_no);
    sprintf (device, "/dev/cxl/afu%d.0s", card_no);
    dn = snap_card_alloc_dev (device, SNAP_VENDOR_ID_IBM, SNAP_DEVICE_ID_SNAP);

    if (NULL == dn) {
        errno = ENODEV;
        VERBOSE0 ("ERROR: snap_card_alloc_dev(%s)\n", device);
        return -1;
    }
    

    /* Read Card Capabilities */ 
    snap_card_ioctl (dn, GET_CARD_TYPE, (unsigned long)&ioctl_data);
    VERBOSE1 ("SNAP on ");

    switch (ioctl_data) {
    case  0:
        VERBOSE1 ("ADKU3");
        break;

    case  1:
        VERBOSE1 ("N250S");
        break;

    case 16:
        VERBOSE1 ("N250SP");
        break;

    default:
        VERBOSE1 ("Unknown");
        break;
    }

    //snap_card_ioctl (dn, GET_SDRAM_SIZE, (unsigned long)&ioctl_data);
    //VERBOSE1 (" Card, %d MB of Card Ram avilable.\n", (int)ioctl_data);

    snap_mmio_read64 (dn, SNAP_S_CIR, &cir);
    VERBOSE0 ("Start of Card Handle: %p Context: %d\n", dn,
              (int) (cir & 0x1ff));

    VERBOSE0 ("======== COMPILE PATTERN FILE ========\n");
    // Compile the regular expression
    patt_src_base = sm_compile_file ("./pattern.txt", &patt_size);
    VERBOSE0 ("======== COMPILE PATTERN FILE DONE ========\n");

    VERBOSE0 ("======== COMPILE PACKET FILE ========\n");
    job_pkt_src_bases = (void**) malloc (num_job_per_thd * num_eng_using * sizeof (void*));
    job_pkt_sizes = (size_t*) malloc (num_job_per_thd * num_eng_using * sizeof (size_t));

    // Compile the packets
    pkt_src_base = regex_scan_file ("./packet.txt", &pkt_size, &pkt_size_for_sw, num_job_per_thd * num_eng_using, job_pkt_src_bases, job_pkt_sizes, &pkt_count);
    VERBOSE0 ("======== COMPILE PACKET FILE DONE ========\n");

    VERBOSE0 ("======== SOFTWARE RUN ========\n");
    // The software run.
    start_time = get_usec();
    regex_ref_run_match();
    elapsed_time = get_usec() - start_time;
    VERBOSE0 ("Software run finished with size %d.\n", (int) pkt_size_for_sw);
    int sw_num_matched_pkt = regex_ref_get_num_matched_pkt();
    printf ("Software run finished with %d matched packets", sw_num_matched_pkt);
    float sw_bw = print_time (elapsed_time, pkt_size_for_sw);
    printf (" end after %lu usec (%0.3f MB/sec)\n", elapsed_time, sw_bw);
    VERBOSE0 ("======== SOFTWARE DONE========\n");
    
    VERBOSE0 ("Start to get action.\n");
    act = get_action (dn, attach_flags, 5 * timeout);

    if (NULL == act) {
        goto __exit1;
    }

    VERBOSE0 ("Finish get action.\n");
    
    hw_version = action_read (dn, SNAP_ACTION_VERS_REG);
    VERBOSE0 ("hw_version: %#x\n", hw_version);

    num_patt_pipes = ( int) ( ( hw_version & 0xFF000000) >> 24);
    num_pkt_pipes =  ( int) ( ( hw_version & 0x00FF0000) >> 16);
    num_engines =    ( int) ( ( hw_version & 0x0000FF00) >> 8);
    revision =       ( int) ( hw_version & 0x000000FF);

    VERBOSE0 ("Running with %d %dx%d regex engine(s), revision: %d\n", num_engines, num_pkt_pipes, num_patt_pipes, revision);

    VERBOSE1 ("======== HARDWARE RUN ========\n");
    
    if (num_eng_using > num_engines) {
	printf ("ERROR: number of engines in command is larger than number of engines. At most %d engines available.\n", num_engines);
	goto fail;
    }

    printf ("Working with %d engines, %d jobs per thread, %d repeating tests.\n", num_eng_using, num_job_per_thd, num_repeat);

    float sum_thd_scan_bw = 0, sum_wkr_bw = 0, sum_total_bw = 0;
    uint64_t sum_buff = 0, sum_scan = 0, sum_cleanup = 0;
    float sum_sd_buff = 0, sum_sd_scan = 0;
    
    for (int i = 0; i < num_repeat; ++i) {
	printf ("------- Iteration %d -------\n", i + 1);
	float thd_scan_bw, wkr_bw, total_bw;
	uint64_t max_buff, max_scan, cleanup_time;
	float sd_buff, sd_scan;

        ERROR_CHECK (start_regex_workers (num_eng_using, num_job_per_thd, patt_src_base, patt_size, pkt_size, job_pkt_src_bases, job_pkt_sizes, pkt_count,
				          dn, act, attach_flags, &thd_scan_bw, &wkr_bw, &total_bw, &max_buff, &sd_buff, &max_scan, &sd_scan, &cleanup_time));

	sum_thd_scan_bw += thd_scan_bw;
	sum_wkr_bw      += wkr_bw;
	sum_total_bw    += total_bw;
	sum_buff        += max_buff;
	sum_sd_buff     += sd_buff;
	sum_scan        += max_scan;
	sum_sd_scan     += sd_scan;
	sum_cleanup     += cleanup_time;

	sleep (1);
    }

    printf ("NUM_ENG: %d\tNUM_JOB: %d\n", num_eng_using, num_job_per_thd);
    printf ("THD SCAN BW: %0.3f (MB/sec)\n", sum_thd_scan_bw / num_repeat);
    printf ("WKR BW:      %0.3f (MB/sec)\n", sum_wkr_bw      / num_repeat);
    printf ("TOTAL BW:    %0.3f (MB/sec)\n", sum_total_bw    / num_repeat);
    printf ("BUFF PREP:   %lu (usec)\t(STD DEV: %0.3f)\n", sum_buff / num_repeat, sum_sd_buff / num_repeat);
    printf ("SCAN:        %lu (usec)\t(STD DEV: %0.3f)\n", sum_scan / num_repeat, sum_sd_scan / num_repeat);
    printf ("CLEANUP:     %lu (usec)\n\n", sum_cleanup / num_repeat);

fail:
    return -1;
    
    if (job_pkt_src_bases) {
	free (job_pkt_src_bases);
    }
    if (job_pkt_sizes) {
	free (job_pkt_sizes);
    }
    free_mem (patt_src_base);
    free_mem (pkt_src_base);
    snap_detach_action (act);
    // Unmap AFU MMIO registers, if previously mapped
    VERBOSE2 ("Free Card Handle: %p\n", dn);
    snap_card_free (dn);
__exit1:
    VERBOSE1 ("End of Test\n"); 
    return 0;
}


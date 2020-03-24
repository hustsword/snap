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
#include <time.h>
#include <sys/time.h>
#include <getopt.h>
#include <ctype.h>

#include <libsnap.h>
#include <snap_tools.h>
#include <snap_s_regs.h>

#include "string_match.h"
//#include "utils/fregex.h"
//#include "regex_ref.h"

/*  defaults */
#define STEP_DELAY      200
#define DEFAULT_MEMCPY_BLOCK    4096
#define DEFAULT_MEMCPY_ITER 1
#define ACTION_WAIT_TIME    10   /* Default in sec */
//#define MAX_NUM_PKT 502400

#define MEGAB       (1024*1024ull)
#define GIGAB       (1024 * MEGAB)

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

#define PATTERN_BODY_LEN	192

#define PACKET_BODY_MAX_LEN (2048 - 64)
#define PACKET_MAX_LEN		2048
#define PACKET_HEAD_LEN		64

static uint32_t PATTERN_ID = 0;
static uint32_t PACKET_ID = 0;
static const char* version = GIT_VERSION;
static  int verbose_level = 0;

static uint64_t get_usec (void)
{
    struct timeval t;

    gettimeofday (&t, NULL);
    return t.tv_sec * 1000000 + t.tv_usec;
}

static uint64_t get_nsec (void)
{
	struct timespec time;
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time);

    return time.tv_sec * 1000000000 + time.tv_nsec;
}

static int get_file_line_count (FILE* fp)
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

#if 0
static void remove_newline (char* str)
{
    char* pos;

    if ((pos = strchr (str, '\n')) != NULL) {
        *pos = '\0';
    } else {
        VERBOSE0 ("Input too long for remove_newline ... ");
        exit (EXIT_FAILURE);
    }
}
#endif

static void print_time (uint64_t elapsed, uint64_t size)
{
    int t;
    float fsize = (float)size / (1024 * 1024);
    float ft;

    if (elapsed > 10000) {
        t = (int)elapsed / 1000;
        ft = (1000 / (float)t) * fsize;
        VERBOSE0 (" end after %d msec (%0.3f MB/sec)\n", t, ft);
    } else {
        t = (int)elapsed;
        ft = (1000000 / (float)t) * fsize;
        VERBOSE0 (" end after %d usec (%0.3f MB/sec)\n", t, ft);
    }
}

static void print_ntime (uint64_t elapsed)
{
    VERBOSE0 ("&&&&&&&&&&&  end after %lu nsec \r\n", elapsed);
}

static void* alloc_mem (int align, size_t size)
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

static void free_mem (void* a)
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

static void* fill_one_packet (const char* in_pkt, int size, void* in_pkt_addr)
{
    unsigned char* pkt_base_addr = in_pkt_addr;
    int pkt_id;
    uint32_t bytes_used = 0;
    //uint16_t pkt_len = size - 1;
	//for test
	uint16_t pkt_len = size - 1 - 64;
	//for test
	//
	uint8_t row_count;
	uint8_t module_marks;
	uint8_t field_type;
	uint8_t field_size;
	
	
	//5A5A5A5A
	for (int i = 0; i < 4; i++) {
		pkt_base_addr[bytes_used] = 0x5A;
		bytes_used++;
	}
	//packet length
	pkt_base_addr[bytes_used] = (pkt_len & 0xFF);
	bytes_used++;
	pkt_base_addr[bytes_used] = ((pkt_len >> 8) & 0xFF);
	bytes_used++;
	//row_count
	row_count = (size - 64) / 32;
	pkt_base_addr[bytes_used] = (row_count & 0xFF);
	bytes_used++;
	//field 1
	module_marks = 15;
	pkt_base_addr[bytes_used] = (module_marks & 0xFF);
	bytes_used++;
	field_type = DATE_TYPE_DOUBLE;
	pkt_base_addr[bytes_used] = (field_type & 0xFF);
	bytes_used++;
	field_size = 8;
	pkt_base_addr[bytes_used] = (field_size & 0xFF);
	bytes_used++;
	//field 2
	module_marks = 15;
	pkt_base_addr[bytes_used] = (module_marks & 0xFF);
	bytes_used++;
	field_type = DATE_TYPE_DOUBLE;
	pkt_base_addr[bytes_used] = (field_type & 0xFF);
	bytes_used++;
	field_size = 8;
	pkt_base_addr[bytes_used] = (field_size & 0xFF);
	bytes_used++;
	//field 3
	module_marks = 15;
	pkt_base_addr[bytes_used] = (module_marks & 0xFF);
	bytes_used++;
	field_type = DATE_TYPE_DOUBLE;
	pkt_base_addr[bytes_used] = (field_type & 0xFF);
	bytes_used++;
	field_size = 8;
	pkt_base_addr[bytes_used] = (field_size & 0xFF);
	bytes_used++;
	//field 4 
	module_marks = 15;
	pkt_base_addr[bytes_used] = (module_marks & 0xFF);
	bytes_used++;
	field_type = DATE_TYPE_DOUBLE;
	pkt_base_addr[bytes_used] = (field_type & 0xFF);
	bytes_used++;
	field_size = 8;
	pkt_base_addr[bytes_used] = (field_size & 0xFF);
	bytes_used++;
	//pad to 60 byte
    memset (pkt_base_addr + bytes_used, 0, (60 - bytes_used));
    bytes_used = 60;
    //The TAG ID
    PACKET_ID++;
    pkt_id = PACKET_ID;
    for (int i = 0; i < 4 ; i++) {
        pkt_base_addr[bytes_used] = ((pkt_id >> (8 * i)) & 0xFF);
        bytes_used++;
    }

	//copy body
    memcpy (pkt_base_addr + bytes_used, in_pkt, PACKET_BODY_MAX_LEN);
    bytes_used += PACKET_BODY_MAX_LEN;

    VERBOSE1("inside bytes_used = %u\r\n", bytes_used);
	
	VERBOSE1("\r\n============================================packet============================================\r\n");

#if 0
    for (int i = 0; i < (int)bytes_used ; i++) {
		VERBOSE1("%02x ", pkt_base_addr[i]);
		if ((i + 1) % 32 == 0){
			VERBOSE1("\r\n");
		}
    }
#else

    for (int i = 0; i < (int)bytes_used ; i++) {
		VERBOSE1("%02x ", pkt_base_addr[bytes_used - i -1]);
		if ((i + 1) % 32 == 0){
			VERBOSE1("\r\n");
		}
    }

#endif
	
    return pkt_base_addr + bytes_used;
}

static void* fill_one_pattern (const unsigned char* in_patt, void* in_patt_addr)
{
	unsigned char* patt_base_addr = in_patt_addr;
    uint32_t bytes_used = 0;
    uint16_t pkt_len = 64*5 - 1;
    int pattern_id;

	uint8_t total_col_num;
	uint8_t result_modle;

	//5A5A5A5A
    for (int i = 0; i < 4; i++) {
        patt_base_addr[bytes_used] = 0x5A;
        bytes_used++;
    }
	//pattern length
	patt_base_addr[bytes_used] = (pkt_len & 0xFF);
	bytes_used++;
	patt_base_addr[bytes_used] = ((pkt_len >> 8) & 0xFF);
	bytes_used++;
	//total_col_num
	total_col_num = 3;
	patt_base_addr[bytes_used] = (total_col_num & 0xFF);
	bytes_used++;
	//result_modle
	result_modle = 1;
	patt_base_addr[bytes_used] = (result_modle & 0xFF);
	bytes_used++;
	//pad to 60 byte
    memset (patt_base_addr + bytes_used, 0, (60 - bytes_used));
    bytes_used = 60;
    //The TAG ID
    PATTERN_ID++;
    pattern_id = PATTERN_ID;
    for (int i = 0; i < 4 ; i++) {
        patt_base_addr[bytes_used] = ((pattern_id >> (8 * i)) & 0xFF);
        bytes_used++;
    }

	//copy body
    memcpy (patt_base_addr + bytes_used, in_patt, PACKET_BODY_MAX_LEN);
    bytes_used += PACKET_BODY_MAX_LEN;

	VERBOSE1("\r\n============================================pattern============================================\r\n");

    for (int i = 0; i < (int)bytes_used ; i++) {
		VERBOSE1("%02x ", patt_base_addr[i]);
		if ((i + 1) % 32 == 0){
			VERBOSE1("\r\n");
		}
    }

    return patt_base_addr + bytes_used;
}


/* Action or Kernel Write and Read are 32 bit MMIO */
static void action_write (struct snap_card* h, uint32_t addr, uint32_t data)
{
    int rc;

    rc = snap_mmio_write32 (h, (uint64_t)addr, data);

    if (0 != rc) {
        VERBOSE0 ("Write MMIO 32 Err\n");
    }

    return;
}

static uint32_t action_read (struct snap_card* h, uint32_t addr)
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

/*
 *  Start Action and wait for Idle.
 */
static int action_wait_idle (struct snap_card* h, int timeout, uint64_t* elapsed)
{
    int rc = ETIME;
    uint64_t t_start;   /* time in usec */
    uint64_t td = 0;    /* Diff time in usec */

    return rc;
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

static void print_control_status (struct snap_card* h)
{
    if (verbose_level > 2) {
        uint32_t reg_data;
        VERBOSE3 (" READ Control and Status Registers: \n");
        reg_data = action_read (h, ACTION_STATUS_L);
        VERBOSE3 ("       STATUS_L = 0x%x\n", reg_data);
        reg_data = action_read (h, ACTION_STATUS_H);
        VERBOSE3 ("       STATUS_H = 0x%x\n", reg_data);
        reg_data = action_read (h, ACTION_CONTROL_L);
        VERBOSE3 ("       CONTROL_L = 0x%x\n", reg_data);
        reg_data = action_read (h, ACTION_CONTROL_H);
        VERBOSE3 ("       CONTROL_H = 0x%x\n", reg_data);
    }
}

static void soft_reset (struct snap_card* h)
{
    // Status[4] to reset
    action_write (h, ACTION_CONTROL_L, 0x00000010);
    action_write (h, ACTION_CONTROL_H, 0x00000000);
    VERBOSE2 (" Write ACTION_CONTROL for soft reset! \n");
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
    uint32_t reg_data1;
	double   result = 0;
	unsigned char *tmp = NULL;

    uint64_t timer = 0;
    uint64_t start_time;
    uint64_t elapsed_time;

    start_time = get_usec();
    
	VERBOSE2 (" ------ String Match Start -------- \n");
    VERBOSE2 (" PATTERN SOURCE ADDR: %p -- SIZE: %d\n", patt_src_base, (int)patt_size);
    VERBOSE2 (" PACKET  SOURCE ADDR: %p -- SIZE: %d\n", pkt_src_base, (int)pkt_size);
    VERBOSE2 (" STAT    DEST   ADDR: %p -- SIZE(max): %d\n", stat_dest_base, (int)stat_size);

    VERBOSE2 (" Start register config! \n");
    print_control_status (h);

    action_write (h, ACTION_PATT_INIT_ADDR_L,
                  (uint32_t) (((uint64_t) patt_src_base) & 0xffffffff));
    action_write (h, ACTION_PATT_INIT_ADDR_H,
                  (uint32_t) ((((uint64_t) patt_src_base) >> 32) & 0xffffffff));
    VERBOSE2 (" Write ACTION_PATT_INIT_ADDR done! \n");

    action_write (h, ACTION_PKT_INIT_ADDR_L,
                  (uint32_t) (((uint64_t) pkt_src_base) & 0xffffffff));
    action_write (h, ACTION_PKT_INIT_ADDR_H,
                  (uint32_t) ((((uint64_t) pkt_src_base) >> 32) & 0xffffffff));
    VERBOSE2 (" Write ACTION_PKT_INIT_ADDR done! \n");

    action_write (h, ACTION_PATT_CARD_DDR_ADDR_L, 0);
    action_write (h, ACTION_PATT_CARD_DDR_ADDR_H, 0);
    VERBOSE2 (" Write ACTION_PATT_CARD_DDR_ADDR done! \n");

    action_write (h, ACTION_STAT_INIT_ADDR_L,
                  (uint32_t) (((uint64_t) stat_dest_base) & 0xffffffff));
    action_write (h, ACTION_STAT_INIT_ADDR_H,
                  (uint32_t) ((((uint64_t) stat_dest_base) >> 32) & 0xffffffff));
    VERBOSE2 (" Write ACTION_STAT_INIT_ADDR done! \n");

    action_write (h, ACTION_PATT_TOTAL_NUM_L,
                  (uint32_t) (((uint64_t) patt_size) & 0xffffffff));
    action_write (h, ACTION_PATT_TOTAL_NUM_H,
                  (uint32_t) ((((uint64_t) patt_size) >> 32) & 0xffffffff));
    VERBOSE2 (" Write ACTION_PATT_TOTAL_NUM done! \n");

    action_write (h, ACTION_PKT_TOTAL_NUM_L,
                  (uint32_t) (((uint64_t) pkt_size) & 0xffffffff));
    action_write (h, ACTION_PKT_TOTAL_NUM_H,
                  (uint32_t) ((((uint64_t) pkt_size) >> 32) & 0xffffffff));
    VERBOSE2 (" Write ACTION_PKT_TOTAL_NUM done! \n");

    action_write (h, ACTION_STAT_TOTAL_SIZE_L,
                  (uint32_t) (((uint64_t) stat_size) & 0xffffffff));
    action_write (h, ACTION_STAT_TOTAL_SIZE_H,
                  (uint32_t) ((((uint64_t) stat_size) >> 32) & 0xffffffff));
    
	
    action_write (h, ACTION_CNF_L,
                  (uint32_t) (((uint64_t) 0xC000) & 0xffffffff));
    action_write (h, ACTION_CNF_H,
                  (uint32_t) ((((uint64_t) 0xC000) >> 32) & 0xffffffff));
	
	
	
	VERBOSE2 (" Write ACTION_STAT_TOTAL_SIZE done! \n");

    // Start copying the pattern from host memory to card
    action_write (h, ACTION_CONTROL_L, 0x00000001);
    action_write (h, ACTION_CONTROL_H, 0x00000000);
    VERBOSE2 (" Write ACTION_CONTROL for pattern copying! \n");

    print_control_status (h);

    do {
        reg_data = action_read (h, ACTION_STATUS_L);
        //VERBOSE3 ("Pattern Phase: polling Status reg with 0X%X\n", reg_data);

        // Status[23:8]
        if ((reg_data & 0x00FFFF00) != 0) {
            VERBOSE0 ("Error code got 0X%X\n", ((reg_data & 0x00FFFF00) >> 8));
            exit (EXIT_FAILURE);
        }

        // Status[0]
        if ((reg_data & 0x00000002) == 0x00000002) {
            VERBOSE1 ("Pattern copy done!\n");
            break;
        }
    } while (1);


    // Start copying the packet from host memory to card
    action_write (h, ACTION_CONTROL_L, 0x00000002);
    action_write (h, ACTION_CONTROL_H, 0x00000000);
    VERBOSE2 (" Write ACTION_CONTROL for packet copying! \n");

    print_control_status (h);

    do {
        reg_data = action_read (h, ACTION_STATUS_L);
        //VERBOSE3 ("Pattern Phase: polling Status reg with 0X%X\n", reg_data);

        // Status[23:8]
        if ((reg_data & 0x00FFFF00) != 0) {
            VERBOSE0 ("Error code got 0X%X\n", ((reg_data & 0x00FFFF00) >> 8));
            exit (EXIT_FAILURE);
        }

        // Status[0]
        if ((reg_data & 0x00000001) == 0x00000001) {
            VERBOSE1 ("cal  done!\n");
            break;
        }
    } while (1);
	printf("==============================packet data used up  \r\n");

	reg_data = action_read (h, ACTION_CAL_RESULT_L);
	reg_data1 = action_read (h, ACTION_CAL_RESULT_H);

	//tmp = (unsigned char *)&result + sizeof(double) - 1;
	tmp = (unsigned char *)&result;
	memcpy( (void *)tmp, (void *)&reg_data, sizeof(reg_data) );
	//tmp -= 4;
	tmp += 4;
	memcpy( (void *)tmp, (void *)&reg_data1, sizeof(reg_data1) );


	reg_data = action_read (h, ACTION_TIMER_L);
	reg_data1 = action_read (h, ACTION_TIMER_H);

	//tmp = (unsigned char *)&timer + sizeof(double) - 1;
	tmp = (unsigned char *)&timer;
	memcpy( (void *)tmp, (void *)&reg_data, sizeof(reg_data) );
	//tmp -= 4;
	tmp += 4;
	memcpy( (void *)tmp, (void *)&reg_data1, sizeof(reg_data1) );
	
	//printf("**********cal timer = %lu  \r\n", timer);
	
	timer = timer*5;

	printf("**********cal timer = %lu  \r\n", timer);

	printf("**********cal result = %lf  \r\n", result);


	elapsed_time = get_usec() - start_time;
	print_time (elapsed_time, 100);


	printf("**********cal   finished\r\n");
	return;

    //start_time = get_usec();
    // Start working control[2:1] = 11
    action_write (h, ACTION_CONTROL_L, 0x00000006);
    action_write (h, ACTION_CONTROL_H, 0x00000000);
    VERBOSE1 (" Write ACTION_CONTROL for working! \n");

    do {
        reg_data = action_read (h, ACTION_STATUS_L);
        VERBOSE1 ("Packet Phase: polling Status reg with 0X%X\n", reg_data);

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
    action_write (h, ACTION_CONTROL_L, 0x00000000);
    action_write (h, ACTION_CONTROL_H, 0x00000000);
    VERBOSE2 (" Write ACTION_CONTROL for stop working! \n");

    // Flush rest data
    action_write (h, ACTION_CONTROL_L, 0x00000008);
    action_write (h, ACTION_CONTROL_H, 0x00000000);
    VERBOSE2 (" Write ACTION_CONTROL for stat flushing! \n");

    do {
        reg_data = action_read (h, ACTION_STATUS_L);

        // Status[23:8]
        if ((reg_data & 0x00FFFF00) != 0) {
            VERBOSE0 ("Error code got 0X%X\n", ((reg_data & 0x00FFFF00) >> 8));
            exit (EXIT_FAILURE);
        }

        // Status[3]
        if ((reg_data & 0x00000008) == 8) {
            VERBOSE2 ("Stat flush done!\n");
            reg_data = action_read (h, ACTION_STATUS_H);
            VERBOSE1 ("Number of matched packets: %d\n", reg_data);
            *num_matched_pkt = reg_data;
            break;
        }

        VERBOSE3 ("Polling Status reg with 0X%X\n", reg_data);
    } while (1);

    // Stop flushing
    action_write (h, ACTION_CONTROL_L, 0x00000000);
    action_write (h, ACTION_CONTROL_H, 0x00000000);
    VERBOSE2 (" Write ACTION_CONTROL for stop working! \n");

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
	#if 1
    VERBOSE3 ("Wait for idle\n");
    rc = action_wait_idle (dnc, timeout, &td);
    VERBOSE3 ("Card in idle\n");
	#endif
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
        VERBOSE0 ("Error: Can not attach Action: %x\n", ACTION_TYPE_STRING_MATCH);
        VERBOSE0 ("       Try to run snap_main tool\n");
    }

    return act;
}

static void usage (const char* prog)
{
    VERBOSE0 ("SNAP String Match (Regular Expression Match) Tool.\n"
              "    Use Option -p and -q for pattern and packet\n"
              "    e.g. %s -p <packet file> -q <pattern file> [-vv] [-I]\n",
              prog);
    VERBOSE0 ("Usage: %s\n"
              "    -h, --help           print usage information\n"
              "    -v, --verbose        verbose mode\n"
              "    -C, --card <cardno>  use this card for operation\n"
              "    -V, --version\n"
              "    -q, --quiet          quiece output\n"
              "    -t, --timeout        Timeout after N sec (default 1 sec)\n"
              "    -I, --irq            Enable Action Done Interrupt (default No Interrupts)\n"
              "    -p, --packet         Packet file for matching\n"
              "    -q, --pattern        Pattern file for matching\n"
              , prog);
}

static void* sm_compile_file (const char* file_path, size_t* size)
{
	char* line = NULL;
	size_t len = 0;
	ssize_t read;
	unsigned char* query6 = NULL;
	char tmp[16] = { 0 };
	int i = 0;
	int j = 0;
	int mark = 0;
	struct tm tmp_time;
    double l_shipdate;
    double l_discount;
    double l_discount_tmp;
    double l_discount_offset = 0.01;
    double l_quantity;

	uint8_t calculate_type = 1;
	uint8_t calculate_date_type = DATE_TYPE_DOUBLE;
	uint8_t calculate_date1 = 1;
	uint8_t calculate_date2 = 3;

	uint8_t op1_type = 0x0F;
	uint8_t op1_datetype = DATE_TYPE_DOUBLE<<4;
	uint8_t op1_datesize = 8;
	uint8_t op2_type = 0x0F;
	uint8_t op2_datetype = DATE_TYPE_DOUBLE<<4;
	uint8_t op2_datesize = 8;
	unsigned char column[64] = { 0 };
    
	uint64_t start_time = 0;
    uint64_t elapsed_time = 0;
    uint64_t sum_time = 0;

	size_t max_alloc_size = PACKET_MAX_LEN;
	//void* patt_src_base = malloc(max_alloc_size);
    void* patt_src_base = alloc_mem (0, max_alloc_size);
    if (patt_src_base == NULL) {
		VERBOSE0("failed to alloc_mem patt_src_base\r\n");
    	return patt_src_base;
    }

    void* patt_src = patt_src_base;

    VERBOSE1 ("PATTERN Source Address Start at 0X%016lX\n", (uint64_t)patt_src);

    FILE* fp = fopen (file_path, "r");

    if (fp == NULL) {
        VERBOSE0 ("PATTERN fle not existed %s\n", file_path);
    	return patt_src_base;
    }

	query6 = malloc(PACKET_BODY_MAX_LEN);
	//query6 = alloc_mem(0, PACKET_BODY_MAX_LEN);
    if (query6 == NULL) {
		VERBOSE0("failed to alloc_mem pPacket_body\r\n");
    	return patt_src_base;
    }
	memset (query6, 0 ,PACKET_BODY_MAX_LEN);

    start_time = get_nsec();

    while ((read = getline (&line, &len, fp)) != -1) {

		elapsed_time = get_nsec() - start_time;
		sum_time += elapsed_time;
		
		//reset para
		op1_type = 0x0F;
		op2_type = 0x0F;
		memset (column, 0, 64);

		//get l_shipdate
		if ( strstr( line, "l_shipdate") != 0 ) {
			for ( i = 0; i < read; i++ ) {
				if (*(line+i) == '>') {
					mark = 1;
					memset( tmp, 0, sizeof(tmp));
					continue;
				} 

				if (*(line+i) == '<') {
					mark = 2;
					memset( tmp, 0, sizeof(tmp));
					continue;
				} 
				
				if (mark == 1 && *(line+i) == '\'') {
					for ( j = 0; j < (read-i); j++ ) {
						if (*(line+i+j+1) != '\'') {
							tmp[j] = *(line+i+j+1);
						} else {
							mark = 0;
							break;
						}
					}

					memset( &tmp_time, 0, sizeof(tmp_time));
					printf("l_shipdate = %s  \r\n", tmp);
					strptime((const char *)&tmp, "%Y-%m-%d", &tmp_time);
					l_shipdate = mktime(&tmp_time) + 3600*8;
					VERBOSE2("l_shipdate min === %.2f \r\n", l_shipdate);
					memcpy (&column[2], &l_shipdate, sizeof(l_shipdate));
				}
				
				if (mark == 2 && *(line+i) == '\'') {
					for ( j = 0; j < (read-i); j++ ) {
						if (*(line+i+j+1) != '\'') {
							tmp[j] = *(line+i+j+1);
						} else {
							mark = 0;
							break;
						}
					}

					memset( &tmp_time, 0, sizeof(tmp_time));
					printf("l_shipdate = %s  \r\n", tmp);
					strptime((const char *)&tmp, "%Y-%m-%d", &tmp_time);
					l_shipdate = mktime(&tmp_time) + 3600*8;
					VERBOSE2("l_shipdate max === %.2f \r\n", l_shipdate);
					memcpy (&column[2+32], &l_shipdate, sizeof(l_shipdate));
					break;
				}
			}

			op1_type = 0x02;		// >=
			op1_datetype = DATE_TYPE_DOUBLE<<4;
			column[0] = (op1_type & 0x0F) | (op1_datetype & 0xF0);
			column[1] = op1_datesize & 0xFF;
			
			op2_type = 0x03;		// <
			op2_datetype = DATE_TYPE_DOUBLE<<4;
			column[0+32] = (op2_type & 0x0F) | (op2_datetype & 0xF0);
			column[1+32] = op2_datesize & 0xFF;
	
			memcpy (query6, &column, sizeof(column));
		
			start_time = get_nsec();
			continue;
		}

		//get l_discount
		if ( strstr( line, "l_discount") != 0 ) {
			for ( i = 0; i < read; i++ ) {
				if (*(line+i) == '.') {
					mark = 1;
					memset( tmp, 0, sizeof(tmp));
					tmp[0] = '0';
					tmp[1] = '.';
					
					continue;
				} 
				
				if (mark == 1 && *(line+i) != '-') {
					for ( j = 0; j < (read-i); j++ ) {
						if (*(line+i+j) != '-') {
					
							tmp[2+j] = *(line+i+j);
							//printf("tmp[%d] = %c	\r\n", j, tmp[j]);
						} else {
							mark = 0;
							break;
						}

					}

					//printf("l_discount = %s  \r\n", tmp);
					l_discount_tmp = atof(tmp);
					//printf("l_discount_tmp === %.2f \r\n", l_discount_tmp);
					l_discount = l_discount_tmp - l_discount_offset;
					VERBOSE2("l_discount  low === %.2f \r\n", l_discount);
					memcpy (&column[2], &l_discount, sizeof(l_discount));
					l_discount = l_discount_tmp + l_discount_offset;
					VERBOSE2("l_discount  high === %.2f \r\n", l_discount);
					memcpy (&column[2+32], &l_discount, sizeof(l_discount));
					break;

				}

			}

			op1_type = 0x02;		// >=
			op1_datetype = DATE_TYPE_DOUBLE<<4;
			column[0] = (op1_type & 0x0F) | (op1_datetype & 0xF0);
			column[1] = op1_datesize & 0xFF;
			
			op2_type = 0x04;		// <=
			op2_datetype = DATE_TYPE_DOUBLE<<4;
			column[0+32] = (op2_type & 0x0F) | (op2_datetype & 0xF0);
			column[1+32] = op2_datesize & 0xFF;
	
			memcpy (query6+64, &column, sizeof(column));
		
			start_time = get_nsec();
			continue;
		}
		
		//get l_quantity
		if ( strstr( line, "l_quantity") != 0 ) {
			for ( i = 0; i < read; i++ ) {
				if (*(line+i) == '<') {
					mark = 1;
					memset( tmp, 0, sizeof(tmp));
					continue;
				} 

				if (mark == 1 && *(line+i) != ' ') {
					for ( j = 0; j < (read-i); j++ ) {
						if (*(line+i+j+1) != ' ') {
					
							tmp[j] = *(line+i+j);
							//printf("tmp[%d] = %c	\r\n", j, tmp[j]);
						} else {
							mark = 0;
							break;
						}
					}

					//printf("l_quantity = %s  \r\n", tmp);
					l_quantity = atof(tmp);
					VERBOSE2("l_quantity === %.2f \r\n", l_quantity);
					memcpy (&column[2], &l_quantity, sizeof(l_quantity));
					break;
				}

			}


			op1_type = 0x03;		// <
			op1_datetype = DATE_TYPE_DOUBLE<<4;
			column[0] = (op1_type & 0x0F) | (op1_datetype & 0xF0);
			column[1] = op1_datesize & 0xFF;

			op2_type = 0x0F;		// <
			op2_datetype = 0;
			column[0+32] = (op2_type & 0x0F) | (op2_datetype & 0xF0);
		
			memcpy (query6+64*2, &column, sizeof(column));

			start_time = get_nsec();
			continue;
		}

		
    }

	printf("**********sum pattern timer = %lu  \r\n", sum_time);

	memset (column, 0, 64);
	column[0] = calculate_type & 0xFF;
	column[1] = calculate_date_type & 0xFF;
	column[2] = calculate_date1 & 0xFF;
	column[33] = calculate_date2 & 0xFF;
	//for test
	column[63] = 0xFF;
	column[62] = 0xFE;
	column[61] = 0xFD;
	column[60] = 0xFC;
	//for test
	
	
	memcpy (query6+64*3, &column, sizeof(column));
	
	patt_src = fill_one_pattern (query6, patt_src);

	free_mem(query6);
	free_mem(line);
	fclose (fp);

	//only one pattern now
	(*size) = 1;
	
    return patt_src_base;
}


static void* sm_scan_file(const char* file_path, size_t* num_pkt)
{
    char* line = NULL;
    size_t len = 0;
    ssize_t read;
    double l_shipdate;
    double l_discount;
    double l_extendedprice;
    double l_quantity;
    char tmp[16] = {0};
    int icount = 0;
	int i = 0;
	int j = 0;
	struct tm tmp_time;
	
	uint64_t start_time = 0;
    uint64_t elapsed_time = 0;
    uint64_t sum_time_io = 0;
    uint64_t sum_time_parse = 0;
    uint64_t sum_time_fill_packet = 0;
	
    uint32_t bytes_used = 0;
	size_t pkt_count = 0;
	char* pPacket_body = NULL;
 
	start_time = get_nsec();

    //printf("Start to count file  number...\r\n");
    FILE* fp = fopen (file_path, "r");
    if (fp == NULL) {
        VERBOSE0 ("PACKET fle not existed %s\n", file_path);
    	return NULL;
    }

    size_t pkt_num = get_file_line_count(fp);

    VERBOSE1("PTK number = %lu\r\n", pkt_num);

    pkt_num = (pkt_num % 62) == 0
			? (pkt_num / 62) 
			: (pkt_num / 62) + 1;

    VERBOSE1("PTK number = %lu\r\n", pkt_num);

    size_t max_alloc_size = pkt_num * 2048;

    //void* pkt_src_base = malloc(max_alloc_size);
    void* pkt_src_base = alloc_mem(0, max_alloc_size);
    if (pkt_src_base == NULL) {
		VERBOSE0("failed to alloc_mem pkt_src_base\r\n");
    	return pkt_src_base;
    }
    unsigned char* pkt_src = pkt_src_base;
    VERBOSE1 ("PACKET Source Address Start at 0X%016lX\n", (uint64_t)pkt_src);
	
	pPacket_body = malloc(PACKET_BODY_MAX_LEN);
	//pPacket_body = alloc_mem(0, PACKET_BODY_MAX_LEN);
    if (pPacket_body == NULL) {
		VERBOSE0("failed to alloc_mem pPacket_body\r\n");
    	return pkt_src_base;
    }
	memset (pPacket_body, 0 ,PACKET_BODY_MAX_LEN);

    fp = fopen (file_path, "r");
    if (fp == NULL) {
		VERBOSE0("failed to open file\r\n");
    	return pkt_src_base;
    }

	//printf("float size === %lu  double size  = %lu\r\n", sizeof(float), sizeof(double));
    

    while ((read = getline (&line, &len, fp)) != -1) {
    	//printf("line === %s  read = %lu\r\n", line, read);
		
		//reset para
		memset( tmp, 0, sizeof(tmp));
		memset( &tmp_time, 0, sizeof(tmp_time));
		icount = 0;
		l_shipdate = 0;
		l_discount = 0;
		l_extendedprice = 0;
		l_quantity = 0;

		elapsed_time = get_nsec() - start_time;
		sum_time_io += elapsed_time;
		
		start_time = get_nsec();
		
		for ( i = 0; i < read; i++ ) {
			//printf("line = %c \r\n", *(line+i));
			if ( *(line+i) == '|' ) {
				icount++;	
				memset( tmp, 0, sizeof(tmp));
				j = 0;
				//printf("icount === %d \r\n", icount);
			}

			//get l_quantity
			if (icount == 4 && j == 0) {
				for ( j = 0; j < (read-i); j++ ) {
					if (*(line+i+j+1) != '|') {

						tmp[j] = *(line+i+j+1);
						//printf("tmp[%d] = %c  \r\n", j, tmp[j]);
					} else {
						break;
					}
				}

				l_quantity = atof(tmp);
			} 

			//get l_extendedprice
			if (icount == 5 && j == 0) {
				for ( j = 0; j < (read-i); j++ ) {
					if (*(line+i+j+1) != '|') {

						tmp[j] = *(line+i+j+1);
						//printf("tmp[%d] = %c  \r\n", j, tmp[j]);
					} else {
						break;
					}
				}

				l_extendedprice = atof(tmp);
			} 

			//get l_discount
			if (icount == 6 && j == 0) {
				for ( j = 0; j < (read-i); j++ ) {
					if (*(line+i+j+1) != '|') {

						tmp[j] = *(line+i+j+1);
						//printf("tmp[%d] = %c  \r\n", j, tmp[j]);
					} else {
						break;
					}
				}

				l_discount = atof(tmp);
			} 

			//get l_shipdate
			if (icount == 10 && j == 0) {
				for ( j = 0; j < (read-i); j++ ) {
					if (*(line+i+j+1) != '|') {

						tmp[j] = *(line+i+j+1);
						//printf("tmp[%d] = %c  \r\n", j, tmp[j]);
					} else {
						break;
					}
				}

				//printf("l_shipdate = %s  \r\n", tmp);
				strptime((const char *)&tmp, "%Y-%m-%d", &tmp_time);
				l_shipdate = mktime(&tmp_time) + 3600*8;
			} 
			
		}	

		//l_shipdate l_discount l_quantity l_extendedprice
		memcpy (pPacket_body + bytes_used, &l_shipdate, sizeof(l_shipdate));
		bytes_used += sizeof(l_shipdate);
		memcpy (pPacket_body + bytes_used, &l_discount, sizeof(l_discount));
		bytes_used += sizeof(l_discount);
		memcpy (pPacket_body + bytes_used, &l_quantity, sizeof(l_quantity));
		bytes_used += sizeof(l_quantity);
		memcpy (pPacket_body + bytes_used, &l_extendedprice, sizeof(l_extendedprice));
		bytes_used += sizeof(l_extendedprice);

		elapsed_time = get_nsec() - start_time;
		sum_time_parse += elapsed_time;
		
		VERBOSE2("l_shipdate === %.2f \r\n", l_shipdate);
		VERBOSE2("l_discount === %.2f \r\n", l_discount);
		VERBOSE2("l_quantity === %.2f \r\n", l_quantity);
		VERBOSE2("l_extendedprice === %.2f \r\n", l_extendedprice);
		
		VERBOSE1("outside bytes_used = %u\r\n", bytes_used);

		pkt_count++;

		start_time = get_nsec();
		
		//fill one packet
		if (pkt_count % 62 == 0) {
			pkt_src = fill_one_packet (pPacket_body, 2048, pkt_src);
			//VERBOSE2 ("PACKET Source Address Start at 0X%016lX\n", (uint64_t)pkt_src);
			(*num_pkt)++;
			memset (pPacket_body, 0 ,PACKET_BODY_MAX_LEN);
			bytes_used = 0;
		}

		elapsed_time = get_nsec() - start_time;
		sum_time_fill_packet += elapsed_time;
		
		start_time = get_nsec();
#if 0
		printf("l_shipdate === %.2f \r\n", l_shipdate);
		printf("l_discount === %.2f \r\n", l_discount);
		printf("l_quantity === %.2f \r\n", l_quantity);
		printf("l_extendedprice === %.2f \r\n", l_extendedprice);

		char pTime[100] = {0};
		printf("pTime = %s  \r\n", pTime);
		time_t testtime = l_shipdate + 3600*8;
		printf("testtime === %.2f \r\n", (double)testtime);
		struct tm* timeinfo;
		timeinfo = gmtime(&testtime);
		strftime(pTime, sizeof(pTime), "%Y-%m-%d", timeinfo);
		printf("pTime = %s  \r\n", pTime);


		printf("tmp_time year=%d mon=%d day=%d \r\n", tmp_time.tm_year, tmp_time.tm_mon, tmp_time.tm_mday);
		printf("timeinfo year=%d mon=%d day=%d \r\n", timeinfo->tm_year, timeinfo->tm_mon, timeinfo->tm_mday);
#endif 
	
	}

	//fill last packet
	if (bytes_used != 0) {
	
		//unsigned char* pkt_base_addr  = pkt_src;
		
		pkt_src = fill_one_packet (pPacket_body, (64+bytes_used), pkt_src);
	
#if 0
		VERBOSE1("\r\n============================================packet============================================\r\n");

		for (int i = 0; i < ((int)bytes_used + 64); i++) {
			VERBOSE0("%02x ", pkt_base_addr[i]);
			if ((i + 1) % 32 == 0){
				VERBOSE0("\r\n");
			}
		}
#endif
		//VERBOSE2 ("PACKET Source Address Start at 0X%016lX\n", (uint64_t)pkt_src);
		(*num_pkt)++;
		memset (pPacket_body, 0 ,PACKET_BODY_MAX_LEN);
		bytes_used = 0;
	}

	free_mem(pPacket_body);
	free_mem(line);
    fclose(fp);
    
	VERBOSE1(" pkt_count  = %lu\r\n", pkt_count);
   
	printf("**********sum packet read file io timer = %lu nsec \r\n", sum_time_io);
	printf("**********sum packet parse timer = %lu nsec  \r\n", sum_time_parse);
	printf("**********sum packet fill  timer = %lu nsec  \r\n", sum_time_fill_packet);
    
	return pkt_src_base;
}


//static int compare_results (size_t num_matched_pkt, void* stat_dest_base, int no_chk_offset)
//{
//    int i = 0, j = 0;
//    uint16_t offset = 0;
//    uint32_t pkt_id = 0;
//    uint32_t patt_id = 0;
//    int rc = 0;
//
//    if ((int)num_matched_pkt != regex_ref_get_num_matched_pkt()) {
//        VERBOSE0 ("ERROR! Num matched packets mismatch\n");
//        VERBOSE0 ("EXPECTED: %d\n", regex_ref_get_num_matched_pkt());
//        VERBOSE0 ("ACTUAL: %d\n", (int)num_matched_pkt);
//        rc = 1;
//    }
//
//    VERBOSE1 ("---- Results (HW: hardware, SW: software) ----\n");
//    VERBOSE1 ("PKT(HW) PATT(HW) OFFSET(HW) PKT(SW) PATT(SW) OFFSET(SW)\n");
//
//    for (i = 0; i < (int)num_matched_pkt; i++) {
//        for (j = 0; j < 4; j++) {
//            patt_id |= (((uint8_t*)stat_dest_base)[i * 10 + j] << j * 8);
//        }
//
//        for (j = 4; j < 8; j++) {
//            pkt_id |= (((uint8_t*)stat_dest_base)[i * 10 + j] << (j % 4) * 8);
//        }
//
//        for (j = 8; j < 10; j++) {
//            offset |= (((uint8_t*)stat_dest_base)[i * 10 + j] << (j % 2) * 8);
//        }
//
//        sm_stat ref_stat = regex_ref_get_result (pkt_id);
//
//        //VERBOSE1("%9d\t%8d\t%9d\t%9d\t%8d\t%9d", pkt_id, patt_id, offset,
//        //        ref_stat.packet_id, ref_stat.pattern_id, ref_stat.offset);
//
//        if ((ref_stat.packet_id != pkt_id) ||
//            (ref_stat.pattern_id != patt_id) ||
//            ((ref_stat.offset != offset) && (no_chk_offset == 0))) {
//            VERBOSE1 ("%7d\t%6d\t%7d\t%7d\t%6d\t%9d", pkt_id, patt_id, offset,
//                      ref_stat.packet_id, ref_stat.pattern_id, ref_stat.offset);
//
//            VERBOSE1 (" MISMATCH!\n");
//            rc = 1;
//        } else {
//            VERBOSE1 ("%7d\t%6d\t%7d\t%7d\t%6d\t%7d", pkt_id, patt_id, offset,
//                      ref_stat.packet_id, ref_stat.pattern_id, ref_stat.offset);
//
//            VERBOSE1 ("    MATCHED!\n");
//        }
//
//        patt_id = 0;
//        pkt_id = 0;
//        offset = 0;
//    }
//
//    return rc;
//}

int main (int argc, char* argv[])
{
    char device[64];
    struct snap_card* dn;   /* lib snap handle */
    int card_no = 0;
    int cmd;
    int rc = 1;
    uint64_t cir;
    int timeout = ACTION_WAIT_TIME;
    //int no_chk_offset = 0;
    snap_action_flag_t attach_flags = 0;
    struct snap_action* act = NULL;
    unsigned long ioctl_data;
    void* patt_src_base = NULL;
    void* pkt_src_base = NULL;
	size_t num_pkt = 0;
	size_t num_patt = 0;
	size_t stat_size = 0;
    uint64_t start_time;
    uint64_t elapsed_time;
    //uint32_t reg_data;

    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            { "card",         required_argument, NULL, 'C' },
            { "verbose",      no_argument,       NULL, 'v' },
            { "help",         no_argument,       NULL, 'h' },
            { "version",      no_argument,       NULL, 'V' },
            { "quiet",        no_argument,       NULL, 'q' },
            { "timeout",      required_argument, NULL, 't' },
            { "irq",          no_argument,       NULL, 'I' },
            { "no_chk_offset", no_argument,       NULL, 'f' },
            { "packet",       required_argument, NULL, 'p' },
            { "pattern",      required_argument, NULL, 'q' },
            { 0,              no_argument,       NULL, 0   },
        };
        cmd = getopt_long (argc, argv, "C:t:p:q:IfqvVh",
                           long_options, &option_index);

        if (cmd == -1) { /* all params processed ? */
            break;
        }

        switch (cmd) {
        case 'v':   /* verbose */
            verbose_level++;
            break;

        case 'V':   /* version */
            VERBOSE0 ("%s\n", version);
            exit (EXIT_SUCCESS);;

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

        case 'f':      /* don't check offset */
            //no_chk_offset = 1;
            break;

        default:
            usage (argv[0]);
            exit (EXIT_FAILURE);
        }
    }

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
	start_time = get_nsec();
    // Compile the regular expression
    patt_src_base = sm_compile_file ("./pattern.txt", &num_patt);
	VERBOSE2("======== num_patt = %lu ========\n", num_patt);
    elapsed_time = get_nsec() - start_time;
    print_ntime (elapsed_time);
    VERBOSE0 ("======== COMPILE PATTERN FILE DONE ========\n");

    VERBOSE0 ("======== COMPILE PACKET FILE ========\n");
	start_time = get_nsec();
    // Compile the packets
	//pkt_src_base = sm_scan_file("./test.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./test10.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./test100.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./test150.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./test200.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./test300.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./test400.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./test500.tbl", &num_pkt);	
	
	pkt_src_base = sm_scan_file("./err.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./testerr.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./testerr100.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./testerr300.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./testerr400.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./testerr500.tbl", &num_pkt);	
	
	//pkt_src_base = sm_scan_file("./test1k.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./test2k.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./test3k.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./test5k.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./test10k.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./test50k.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./test250k.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./test1m.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./test1000000.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./test1000000.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./lineitem.tbl", &num_pkt);	
	
	
	//pkt_src_base = sm_scan_file("./test2k.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./test4k.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./test8k.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./test16k.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./test32k.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./test64k.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./test128k.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./test256k.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./test512k.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./test1024k.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./test10240k.tbl", &num_pkt);	
	
	
	//pkt_src_base = sm_scan_file("./test500_pkt.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./test1k_pkt.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./test5k_pkt.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./test5k_pkt.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./test10k_pkt.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./test50k_pkt.tbl", &num_pkt);	
	//pkt_src_base = sm_scan_file("./test100k_pkt.tbl", &num_pkt);	
	
	VERBOSE2("======== num_pkt = %lu ========\n", num_pkt);
    elapsed_time = get_nsec() - start_time;
    print_ntime (elapsed_time);
    VERBOSE0 ("======== COMPILE PACKET FILE DONE ========\n");

	num_patt = num_patt * 2048;
	num_pkt = num_pkt * 2048;

#if 0
    VERBOSE0 ("======== SOFTWARE RUN ========\n");
    // The software run.
    start_time = get_usec();
    //regex_ref_run_match();
    elapsed_time = get_usec() - start_time;
    VERBOSE0 ("Software run finished.\n");
    print_time (elapsed_time, 100);
    VERBOSE0 ("======== SOFTWARE DONE========\n");
#endif

    VERBOSE0 ("Start to get action.\n");
    act = get_action (dn, attach_flags, 5 * timeout);

    if (NULL == act) {
        goto __exit1;
    }

    VERBOSE0 ("Finish get action.\n");

    // Reset the hardware
    soft_reset (dn);

    VERBOSE1 ("======== HARDWARE RUN ========\n");
  
    printf("==========*******start FPGA work*********==========\r\n");
	start_time = get_nsec();
	VERBOSE1 ("======== HARDWARE RUN #0 ========\n");
	rc = sm_scan (dn, timeout,
				  patt_src_base,
				  pkt_src_base,
				  NULL,
				  NULL,
				  num_patt,
				  num_pkt,
				  stat_size);
	elapsed_time = get_nsec() - start_time;
    print_ntime (elapsed_time);

    printf("==========*******stop FPGA work*********==========\r\n");
	VERBOSE1 ("Finish sm_scan with  matched packets.\n");
#if 0
	for (int i = 0; i < 100; i++) {
        pkt_src_base_0 = alloc_mem (64, pkt_size);
        memcpy (pkt_src_base_0, pkt_src_base, pkt_size);
        pkt_src_base_1 = alloc_mem (64, pkt_size);
        memcpy (pkt_src_base_1, pkt_src_base, pkt_size);
        pkt_src_base_2 = alloc_mem (64, pkt_size);
        memcpy (pkt_src_base_2, pkt_src_base, pkt_size);
        pkt_src_base_3 = alloc_mem (64, pkt_size);
        memcpy (pkt_src_base_3, pkt_src_base, pkt_size);

        stat_dest_base_0 = alloc_mem (64, stat_size);
        memset (stat_dest_base_0, 0, stat_size);
        stat_dest_base_1 = alloc_mem (64, stat_size);
        memset (stat_dest_base_1, 0, stat_size);
        stat_dest_base_2 = alloc_mem (64, stat_size);
        memset (stat_dest_base_2, 0, stat_size);
        stat_dest_base_3 = alloc_mem (64, stat_size);
        memset (stat_dest_base_3, 0, stat_size);


        start_time = get_usec();
        VERBOSE1 ("======== HARDWARE RUN #0 ========\n");
        rc = sm_scan (dn, timeout,
                      patt_src_base,
                      pkt_src_base_0,
                      stat_dest_base_0,
                      &num_matched_pkt,
                      patt_size,
                      pkt_size,
                      stat_size);
        elapsed_time = get_usec() - start_time;
        // pkt_size_for_sw is the real size without hardware specific 64B header
        print_time (elapsed_time, pkt_size_for_sw);

        VERBOSE1 ("Finish sm_scan with %d matched packets.\n", (int)num_matched_pkt);

        start_time = get_usec();
        VERBOSE1 ("======== HARDWARE RUN #1 ========\n");
        rc = sm_scan (dn, timeout,
                      patt_src_base,
                      pkt_src_base_1,
                      stat_dest_base_1,
                      &num_matched_pkt,
                      patt_size,
                      pkt_size,
                      stat_size);
        elapsed_time = get_usec() - start_time;
        // pkt_size_for_sw is the real size without hardware specific 64B header
        print_time (elapsed_time, pkt_size_for_sw);

        VERBOSE1 ("Finish sm_scan with %d matched packets.\n", (int)num_matched_pkt);

        start_time = get_usec();
        VERBOSE1 ("======== HARDWARE RUN #2 ========\n");
        rc = sm_scan (dn, timeout,
                      patt_src_base,
                      pkt_src_base_2,
                      stat_dest_base_2,
                      &num_matched_pkt,
                      patt_size,
                      pkt_size,
                      stat_size);
        elapsed_time = get_usec() - start_time;
        // pkt_size_for_sw is the real size without hardware specific 64B header
        print_time (elapsed_time, pkt_size_for_sw);

        VERBOSE1 ("Finish sm_scan with %d matched packets.\n", (int)num_matched_pkt);

        start_time = get_usec();
        VERBOSE1 ("======== HARDWARE RUN #3 ========\n");
        rc = sm_scan (dn, timeout,
                      patt_src_base,
                      pkt_src_base_3,
                      stat_dest_base_3,
                      &num_matched_pkt,
                      patt_size,
                      pkt_size,
                      stat_size);
        elapsed_time = get_usec() - start_time;
        // pkt_size_for_sw is the real size without hardware specific 64B header
        print_time (elapsed_time, pkt_size_for_sw);

        VERBOSE1 ("Finish sm_scan with %d matched packets.\n", (int)num_matched_pkt);

        VERBOSE0 ("Cleanup memories");
        start_time = get_usec();
        free_mem (pkt_src_base_0);
        free_mem (pkt_src_base_1);
        free_mem (pkt_src_base_2);
        free_mem (pkt_src_base_3);
        free_mem (stat_dest_base_0);
        free_mem (stat_dest_base_1);
        free_mem (stat_dest_base_2);
        free_mem (stat_dest_base_3);
        elapsed_time = get_usec() - start_time;
        print_time (elapsed_time, 0);
    }

#endif

    VERBOSE1 ("======== HARDWARE DONE========\n");
   
    free_mem (pkt_src_base);
    free_mem (patt_src_base);
    snap_detach_action (act);
    // Unmap AFU MMIO registers, if previously mapped
    VERBOSE2 ("Free Card Handle: %p\n", dn);
    snap_card_free (dn);
__exit1:
    VERBOSE1 ("End of Test rc: %d\n", rc);
    return rc;
}


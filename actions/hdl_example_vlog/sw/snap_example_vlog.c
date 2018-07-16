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

#include "snap_example_vlog.h"

/*  defaults */
#define STEP_DELAY      200
#define DEFAULT_MEMCPY_BLOCK    4096
#define DEFAULT_MEMCPY_ITER 1
#define ACTION_WAIT_TIME    10   /* Default in sec */

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

static const char* version = GIT_VERSION;
static  int verbose_level = 0;

static uint64_t get_usec (void)
{
    struct timeval t;

    gettimeofday (&t, NULL);
    return t.tv_sec * 1000000 + t.tv_usec;
}

//static void remove_newline(char* str)
//{
//    char *pos;
//    if ((pos=strchr(str, '\n')) != NULL) {
//        *pos = '\0';
//    } else {
//        VERBOSE0("Input too long for remove_newline ... ");
//        exit(EXIT_FAILURE);
//    }
//}

//static void print_time (uint64_t elapsed, uint64_t size)
//{
//    int t;
//    float fsize = (float)size / (1024 * 1024);
//    float ft;
//
//    if (elapsed > 10000) {
//        t = (int)elapsed / 1000;
//        ft = (1000 / (float)t) * fsize;
//        VERBOSE1 (" end after %d msec (%0.3f MB/sec)\n" , t, ft);
//    } else {
//        t = (int)elapsed;
//        ft = (1000000 / (float)t) * fsize;
//        VERBOSE1 (" end after %d usec (%0.3f MB/sec)\n", t, ft);
//    }
//}

static void* alloc_mem (int align, int size)
{
    void* a;
    int size2 = size + align;

    VERBOSE2 ("%s Enter Align: %d Size: %d\n", __func__, align, size);

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

static uint32_t action_read(struct snap_card* h, uint32_t addr)
{
	int rc;
	uint32_t data;

	rc = snap_mmio_read32(h, (uint64_t)addr, &data);
	if (0 != rc)
		VERBOSE0("Read MMIO 32 Err\n");
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
    uint32_t cnt = 0;

    VERBOSE0 (" ------ String Match Start -------- \n");
    VERBOSE0 (" PATTERN SOURCE ADDR: %p -- SIZE: %d\n", patt_src_base, (int)patt_size);
    VERBOSE0 (" PACKET  SOURCE ADDR: %p -- SIZE: %d\n", pkt_src_base, (int)pkt_size);
    VERBOSE0 (" STAT    DEST   ADDR: %p -- SIZE(max): %d\n", stat_dest_base, (int)stat_size);

    VERBOSE0 (" Start register config! \n");

    action_write (h, ACTION_PATT_INIT_ADDR_L,
                  (uint32_t) (((uint64_t) patt_src_base) & 0xffffffff));
    action_write (h, ACTION_PATT_INIT_ADDR_H,
                  (uint32_t) ((((uint64_t) patt_src_base) >> 32) & 0xffffffff));
    VERBOSE1 (" Write ACTION_PATT_INIT_ADDR done! \n");

    action_write (h, ACTION_PKT_INIT_ADDR_L,
                  (uint32_t) (((uint64_t) pkt_src_base) & 0xffffffff));
    action_write (h, ACTION_PKT_INIT_ADDR_H,
                  (uint32_t) ((((uint64_t) pkt_src_base) >> 32) & 0xffffffff));
    VERBOSE1 (" Write ACTION_PKT_INIT_ADDR done! \n");

    action_write (h, ACTION_PATT_CARD_DDR_ADDR_L, 0);
    action_write (h, ACTION_PATT_CARD_DDR_ADDR_H, 0);
    VERBOSE1 (" Write ACTION_PATT_CARD_DDR_ADDR done! \n");

    action_write (h, ACTION_STAT_INIT_ADDR_L,
                  (uint32_t) (((uint64_t) stat_dest_base) & 0xffffffff));
    action_write (h, ACTION_STAT_INIT_ADDR_H,
                  (uint32_t) ((((uint64_t) stat_dest_base) >> 32) & 0xffffffff));
    VERBOSE1 (" Write ACTION_STAT_INIT_ADDR done! \n");

    action_write (h, ACTION_PATT_TOTAL_NUM_L,
                  (uint32_t) (((uint64_t) patt_size) & 0xffffffff));
    action_write (h, ACTION_PATT_TOTAL_NUM_H,
                  (uint32_t) ((((uint64_t) patt_size) >> 32) & 0xffffffff));
    VERBOSE1 (" Write ACTION_PATT_TOTAL_NUM done! \n");

    action_write (h, ACTION_PKT_TOTAL_NUM_L,
                  (uint32_t) (((uint64_t) pkt_size) & 0xffffffff));
    action_write (h, ACTION_PKT_TOTAL_NUM_H,
                  (uint32_t) ((((uint64_t) pkt_size) >> 32) & 0xffffffff));
    VERBOSE1 (" Write ACTION_PKT_TOTAL_NUM done! \n");

    action_write (h, ACTION_STAT_TOTAL_SIZE_L,
                  (uint32_t) (((uint64_t) stat_size) & 0xffffffff));
    action_write (h, ACTION_STAT_TOTAL_SIZE_H,
                  (uint32_t) ((((uint64_t) stat_size) >> 32) & 0xffffffff));
    VERBOSE1 (" Write ACTION_STAT_TOTAL_SIZE done! \n");

    // Start copying the pattern from host memory to card
    action_write (h, ACTION_CONTROL_L, 0x00000001);
    action_write (h, ACTION_CONTROL_H, 0x00000000);
    VERBOSE1 (" Write ACTION_CONTROL for pattern copying! \n");

    cnt = 0;
    do {
        reg_data = action_read(h, ACTION_STATUS_L);

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

        VERBOSE3("Polling Status reg with 0X%X\n", reg_data);
        cnt++;
    } while (cnt < 16);

    // Start working control[2:1] = 11
    action_write (h, ACTION_CONTROL_L, 0x00000006);
    action_write (h, ACTION_CONTROL_H, 0x00000000);
    VERBOSE1 (" Write ACTION_CONTROL for working! \n");

    cnt = 0;
    do {
        reg_data = action_read(h, ACTION_STATUS_L);

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

            reg_data = action_read(h, ACTION_STATUS_H);
            VERBOSE1 ("%d bytes of valid stat data transfered!\n", reg_data);

            break;
        }

        cnt++;

        VERBOSE3("Polling Status reg with 0X%X\n", reg_data);
    } while (cnt < 16);

    // Stop working
    action_write (h, ACTION_CONTROL_L, 0x00000000);
    action_write (h, ACTION_CONTROL_H, 0x00000000);
    VERBOSE1 (" Write ACTION_CONTROL for stop working! \n");

    // Flush rest data 
    action_write (h, ACTION_CONTROL_L, 0x00000008);
    action_write (h, ACTION_CONTROL_H, 0x00000000);
    VERBOSE1 (" Write ACTION_CONTROL for stat flushing! \n");

    cnt = 0;
    do {
        reg_data = action_read(h, ACTION_STATUS_L);

        // Status[23:8]
        if ((reg_data & 0x00FFFF00) != 0) {
            VERBOSE0 ("Error code got 0X%X\n", ((reg_data & 0x00FFFF00) >> 8));
            exit (EXIT_FAILURE);
        }

        // Status[3]
        if ((reg_data & 0x00000008) == 8) {
            VERBOSE1 ("Stat flush done!\n");
            reg_data = action_read(h, ACTION_STATUS_H);
            VERBOSE1 ("Number of matched packets: %d\n", reg_data);
            *num_matched_pkt = reg_data;
            break;
        }

        VERBOSE3("Polling Status reg with 0X%X\n", reg_data);
        cnt++;
    } while (cnt < 16);

    // Stop flushing 
    action_write (h, ACTION_CONTROL_L, 0x00000000);
    action_write (h, ACTION_CONTROL_H, 0x00000000);
    VERBOSE1 (" Write ACTION_CONTROL for stop working! \n");

    // Wait for transaction to be done.
    usleep(100000);

    int count = 0;
    do {
        VERBOSE3 (" Draining %i! \n", count);
        reg_data = action_read(h, ACTION_STATUS_L);
        count++;
    } while (count < 50);

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
    VERBOSE1 ("Wait for idle\n");
    rc = action_wait_idle (dnc, timeout, &td);
    VERBOSE1 ("Card in idle\n");

    if (0 != rc) {
        return rc;
    }

    return rc;
}

static struct snap_action* get_action (struct snap_card* handle,
                                       snap_action_flag_t flags, int timeout)
{
    struct snap_action* act;

    act = snap_attach_action (handle, ACTION_TYPE_SNAP_EXAMPLE_VLOG,
                              flags, timeout);

    if (NULL == act) {
        VERBOSE0 ("Error: Can not attach Action: %x\n", ACTION_TYPE_SNAP_EXAMPLE_VLOG);
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

int main (int argc, char* argv[])
{
    char device[64];
    struct snap_card* dn;   /* lib snap handle */
    int card_no = 0;
    int cmd;
    int rc = 1;
    uint64_t cir;
    int timeout = ACTION_WAIT_TIME;
    snap_action_flag_t attach_flags = 0;
    struct snap_action* act = NULL;
    unsigned long ioctl_data;
    size_t num_matched_pkt = 0;
    void* pkt_src_base = alloc_mem(64, 8);
    void* patt_src_base = alloc_mem(64, 8);
    void* stat_dest_base = alloc_mem(64, 8);

    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            { "card",     required_argument, NULL, 'C' },
            { "verbose",  no_argument,       NULL, 'v' },
            { "help",     no_argument,       NULL, 'h' },
            { "version",  no_argument,       NULL, 'V' },
            { "quiet",    no_argument,       NULL, 'q' },
            { "timeout",  required_argument, NULL, 't' },
            { "irq",      no_argument,       NULL, 'I' },
            { "packet",   no_argument,       NULL, 'p' },
            { "pattern",  no_argument,       NULL, 'q' },
            { 0,          no_argument,       NULL, 0   },
        };
        cmd = getopt_long (argc, argv, "C:t:p:q:IqvVh",
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
            timeout = strtol (optarg, (char**)NULL, 0); /* in sec */
            break;

        case 'I':      /* irq */
            attach_flags = SNAP_ACTION_DONE_IRQ | SNAP_ATTACH_IRQ;
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

    VERBOSE0 ("Start to get action.\n");

    act = get_action (dn, attach_flags, 5 * timeout);

    if (NULL == act) {
        goto __exit1;
    }

    VERBOSE0 ("Finish get action.\n");

    VERBOSE0 ("Start sm_scan.\n");
    rc = sm_scan (dn, timeout,
                  pkt_src_base,
                  patt_src_base, 
                  stat_dest_base,
                  &num_matched_pkt,
                  0,
                  0,
                  0);

    snap_detach_action (act);

__exit1:
    // Unmap AFU MMIO registers, if previously mapped
    VERBOSE2 ("Free Card Handle: %p\n", dn);
    snap_card_free (dn);

    free_mem(pkt_src_base);
    free_mem(patt_src_base);
    free_mem(stat_dest_base);

    VERBOSE1 ("End of Test rc: %d\n", rc);
    return rc;
}

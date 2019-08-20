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


#include <snap_tools.h>
#include <snap_s_regs.h>

#include "card_io.h"

static int verbose_level = 0;

/* Action or Kernel Write and Read are 32 bit MMIO */
void action_write (struct snap_card* h, uint32_t addr, uint32_t data, int id)
{
    int rc;

    rc = snap_mmio_write32 (h, (uint64_t)REG (addr, id), data);

    if (0 != rc) {
        VERBOSE0 ("Write MMIO 32 Err\n");
    }

    return;
}

uint32_t action_read (struct snap_card* h, uint32_t addr, int id)
{
    int rc;
    uint32_t data;

    rc = snap_mmio_read32 (h, (uint64_t)REG (addr, id), &data);

    if (0 != rc) {
        VERBOSE0 ("Read MMIO 32 Err\n");
    }

    return data;
}

/*
 *  Start Action and wait for Idle.
 */
int action_wait_idle (struct snap_card* h, int timeout)
{
    int rc = ETIME;

    /* FIXME Use struct snap_action and not struct snap_card */
    snap_action_start ((struct snap_action*)h);

    /* Wait for Action to go back to Idle */
    rc = snap_action_completed ((struct snap_action*)h, NULL, timeout);

    if (rc) {
        rc = 0;    /* Good */
    } else {
        VERBOSE0 ("Error. Timeout while Waiting for Idle\n");
    }

    return rc;
}


void soft_reset (struct snap_card* h, int id)
{
    // Status[4] to reset
    action_write (h, ACTION_CONTROL_L, 0x00000010, id);
    action_write (h, ACTION_CONTROL_H, 0x00000000, id);
    VERBOSE2 (" Write ACTION_CONTROL for soft reset! \n");
    action_write (h, ACTION_CONTROL_L, 0x00000000, id);
    action_write (h, ACTION_CONTROL_H, 0x00000000, id);
}

void print_control_status (struct snap_card* h, int id)
{
    if (verbose_level > 2) {
        uint32_t reg_data;
        VERBOSE3 (" READ Control and Status Registers: \n");
        reg_data = action_read (h, ACTION_STATUS_L, id);
        VERBOSE3 ("       STATUS_L = 0x%x\n", reg_data);
        reg_data = action_read (h, ACTION_STATUS_H, id);
        VERBOSE3 ("       STATUS_H = 0x%x\n", reg_data);
        reg_data = action_read (h, ACTION_CONTROL_L, id);
        VERBOSE3 ("       CONTROL_L = 0x%x\n", reg_data);
        reg_data = action_read (h, ACTION_CONTROL_H, id);
        VERBOSE3 ("       CONTROL_H = 0x%x\n", reg_data);
    }
}

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


int action_regex (struct snap_card* h,
                  void* patt_src_base,
                  void* pkt_src_base,
                  void* stat_dest_base,
                  size_t* num_matched_pkt,
                  size_t patt_size,
                  size_t pkt_size,
                  size_t stat_size,
                  int id)
{
    uint32_t reg_data;
    int64_t count = 0;

    VERBOSE2 (" ------ Regular Expression Start -------- \n");
    VERBOSE2 (" PATTERN SOURCE ADDR: %p -- SIZE: %d\n", patt_src_base, (int)patt_size);
    VERBOSE2 (" PACKET  SOURCE ADDR: %p -- SIZE: %d\n", pkt_src_base, (int)pkt_size);
    VERBOSE2 (" STAT    DEST   ADDR: %p -- SIZE(max): %d\n", stat_dest_base, (int)stat_size);

    VERBOSE2 (" Start register config! \n");
    print_control_status (h, eng_id);

    action_write (h, ACTION_PATT_INIT_ADDR_L,
                  (uint32_t) (((uint64_t) patt_src_base) & 0xffffffff), id);
    action_write (h, ACTION_PATT_INIT_ADDR_H,
                  (uint32_t) ((((uint64_t) patt_src_base) >> 32) & 0xffffffff), id);

    action_write (h, ACTION_PKT_INIT_ADDR_L,
                  (uint32_t) (((uint64_t) pkt_src_base) & 0xffffffff), id);
    action_write (h, ACTION_PKT_INIT_ADDR_H,
                  (uint32_t) ((((uint64_t) pkt_src_base) >> 32) & 0xffffffff), id);

    action_write (h, ACTION_PATT_CARD_DDR_ADDR_L, 0, id);
    action_write (h, ACTION_PATT_CARD_DDR_ADDR_H, 0, id);

    action_write (h, ACTION_STAT_INIT_ADDR_L,
                  (uint32_t) (((uint64_t) stat_dest_base) & 0xffffffff), id);
    action_write (h, ACTION_STAT_INIT_ADDR_H,
                  (uint32_t) ((((uint64_t) stat_dest_base) >> 32) & 0xffffffff), id);

    action_write (h, ACTION_PATT_TOTAL_NUM_L,
                  (uint32_t) (((uint64_t) patt_size) & 0xffffffff), id);
    action_write (h, ACTION_PATT_TOTAL_NUM_H,
                  (uint32_t) ((((uint64_t) patt_size) >> 32) & 0xffffffff), id);

    action_write (h, ACTION_PKT_TOTAL_NUM_L,
                  (uint32_t) (((uint64_t) pkt_size) & 0xffffffff), id);
    action_write (h, ACTION_PKT_TOTAL_NUM_H,
                  (uint32_t) ((((uint64_t) pkt_size) >> 32) & 0xffffffff), id);

    action_write (h, ACTION_STAT_TOTAL_SIZE_L,
                  (uint32_t) (((uint64_t) stat_size) & 0xffffffff), id);
    action_write (h, ACTION_STAT_TOTAL_SIZE_H,
                  (uint32_t) ((((uint64_t) stat_size) >> 32) & 0xffffffff), id);

    // Start copying the pattern from host memory to card
    action_write (h, ACTION_CONTROL_L, 0x00000001, id);
    action_write (h, ACTION_CONTROL_H, 0x00000000, id);

    print_control_status (h, id);

    count = 0;

    do {
        reg_data = action_read (h, ACTION_STATUS_L, id);

        // Status[23:8]
        if ((reg_data & 0x00FFFF00) != 0) {
            VERBOSE0 ("Error code got 0X%X\n", ((reg_data & 0x00FFFF00) >> 8));
            return -1;
        }

        // Status[0]
        if ((reg_data & 0x00000001) == 1) {
            VERBOSE1 ("Pattern copy done!\n");
            break;
        }

        usleep (1000);

        count ++;

        if ((count % 5000) == 0) {
            VERBOSE0 ("Heart beat on hardware pattern polling");
        }
    } while (1);

    // Start working control[2:1] = 11
    action_write (h, ACTION_CONTROL_L, 0x00000006, id);
    action_write (h, ACTION_CONTROL_H, 0x00000000, id);

    count = 0;

    do {
        reg_data = action_read (h, ACTION_STATUS_L, id);

        // Status[23:8]
        if ((reg_data & 0x00FFFF00) != 0) {
            VERBOSE0 ("Error code got 0X%X\n", ((reg_data & 0x00FFFF00) >> 8));
            return -1;
        }

        // Status[0]
        if ((reg_data & 0x00000010) != 0) {
            VERBOSE0 ("Error status got 0X%X\n", (reg_data & 0x00000010));
            return -1;
        }

        if ((reg_data & 0x00000006) == 6) {
            break;
        }

        usleep (1000);

        count ++;

        if ((count % 5000) == 0) {
            VERBOSE0 ("Heart beat on hardware status polling");
        }
    } while (1);

    VERBOSE1 ("Work done!\n");

    // Stop working
    action_write (h, ACTION_CONTROL_L, 0x00000000, id);
    action_write (h, ACTION_CONTROL_H, 0x00000000, id);

    // Flush rest data
    action_write (h, ACTION_CONTROL_L, 0x00000008, id);
    action_write (h, ACTION_CONTROL_H, 0x00000000, id);

    count = 0;

    do {
        reg_data = action_read (h, ACTION_STATUS_L, id);

        // Status[23:8]
        if ((reg_data & 0x00FFFF00) != 0) {
            VERBOSE0 ("Error code got 0X%X\n", ((reg_data & 0x00FFFF00) >> 8));
            return -1;
        }

        // Status[3]
        if ((reg_data & 0x00000008) == 8) {
            reg_data = action_read (h, ACTION_STATUS_H, id);
            *num_matched_pkt = reg_data;
            break;
        }

        count ++;

        if ((count % 5000) == 0) {
            VERBOSE0 ("Heart beat on hardware draining polling");
        }
    } while (1);

    VERBOSE1 ("flushing done!\n");

    // Stop flushing
    action_write (h, ACTION_CONTROL_L, 0x00000000, id);
    action_write (h, ACTION_CONTROL_H, 0x00000000, id);

    return 0;
}


int capi_regex_scan_internal (struct snap_card* dnc,
                              int timeout,
                              void* patt_src_base,
                              void* pkt_src_base,
                              void* stat_dest_base,
                              size_t* num_matched_pkt,
                              size_t patt_size,
                              size_t pkt_size,
                              size_t stat_size,
                              int id)
{
    int rc = action_regex (dnc, patt_src_base, pkt_src_base, stat_dest_base, num_matched_pkt,
                           patt_size, pkt_size, stat_size, id);

    if (0 != rc) {
        return rc;
    }

    rc = action_wait_idle (dnc, timeout);

    return rc;
}


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

#ifndef HARDWAREMANAGER_H_h
#define HARDWAREMANAGER_H_h

#include <iostream>
#include <sstream>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <libsnap.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "pg_capi_internal.h"
#ifdef __cplusplus
}
#endif

typedef snap_card CAPICard;
typedef snap_action CAPIAction;

class HardwareManager
{
public:
    // The Constructor of hardware manager
    HardwareManager (int in_card_num);
    HardwareManager (int in_card_num, int in_timeout_sec, int in_timeout_usec);

    // The Destructor of hardware manager
    ~HardwareManager();

    // initialize the card
    int init();

    // Read the register
    uint32_t reg_read (uint32_t in_addr);

    // Write the register
    void reg_write (uint32_t in_addr, uint32_t in_data);

    // Clean up the hardware related resources
    void cleanup();

    // Wait interrupt
    int wait_interrupt();

    // Get the CAPI context
    CAPIContext* get_context();

private:
    // The card number
    int m_card_num;

    // The CAPI card handler
    CAPICard* m_capi_card;

    // The CAPI action handler
    CAPIAction* m_capi_action;

    // The flags to attach the action
    snap_action_flag_t m_attach_flags;

    // The struct to hold all CAPI card related handlers
    CAPIContext* m_context;

    // Timeout value before waiting the action attached
    int m_timeout_sec;
    int m_timeout_usec;
};

typedef boost::shared_ptr<HardwareManager> HardwareManagerPtr;

#endif

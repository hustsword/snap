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

#include "HardwareManager.h"
#include "constants.h"

HardwareManager::HardwareManager (int in_card_num)
    : m_card_num (in_card_num),
      m_capi_card (NULL),
      m_capi_action (NULL),
      m_attach_flags ((snap_action_flag_t)0),
      m_context (NULL),
      m_timeout_sec (0),
      m_timeout_usec (1000)
{
}

HardwareManager::HardwareManager (int in_card_num, int in_timeout_sec, int in_timeout_usec)
    : m_card_num (in_card_num),
      m_capi_card (NULL),
      m_capi_action (NULL),
      m_attach_flags ((snap_action_flag_t)0),
      m_context (NULL),
      m_timeout_sec (in_timeout_sec),
      m_timeout_usec (in_timeout_usec)
{
}

HardwareManager::~HardwareManager()
{
}

int HardwareManager::init()
{
    m_context = (CAPIContext*) palloc0 (sizeof (CAPIContext));

    if (capi_regex_context_init (m_context)) {
        return -1;
    }

    m_capi_card = m_context->dn;
    m_capi_action = m_context->act;
    m_attach_flags = m_context->attach_flags;

    return 0;
}

void HardwareManager::reg_write (uint32_t in_addr, uint32_t in_data)
{
    action_write (m_capi_card, in_addr, in_data);
    return;
}

uint32_t HardwareManager::reg_read (uint32_t in_addr)
{
    return action_read (m_capi_card, in_addr);
}

void HardwareManager::cleanup()
{
    snap_detach_action (m_capi_action);
    snap_card_free (m_capi_card);

    if (m_context) {
        pfree (m_context);
    }

    elog (INFO, "Deattach the card.");
}

int HardwareManager::wait_interrupt()
{
    // TODO: not implemented yet
    //if (snap_action_wait_interrupt (m_capi_action, m_timeout_sec, m_timeout_usec)) {
    //    //std::cout << "Retry waiting interrupt ... " << std::endl;
    //    return -1;
    //}

    return 0;
}

CAPIContext* HardwareManager::get_context()
{
    return m_context;
}


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
      //m_context (NULL),
      m_timeout_sec (0),
      m_timeout_usec (1000)
{
    printf("create hardware manager\n");
}

HardwareManager::HardwareManager (int in_card_num, int in_timeout_sec, int in_timeout_usec)
    : m_card_num (in_card_num),
      m_capi_card (NULL),
      m_capi_action (NULL),
      m_attach_flags ((snap_action_flag_t)0),
      //m_context (NULL),
      m_timeout_sec (in_timeout_sec),
      m_timeout_usec (in_timeout_usec)
{
    printf("create hardware manager\n");
}

HardwareManager::~HardwareManager()
{
}

int HardwareManager::init()
{
    /*
    m_context = (CAPIContext*) palloc0 (sizeof (CAPIContext));

    if (capi_regex_context_init (m_context)) {
        return -1;
    }
    */

    // Init the job descriptor
    m_timeout = ACTION_WAIT_TIME;

    // Prepare the card and action
    printf ("Open Card: %d", m_card_num);
    sprintf (m_device, "/dev/cxl/afu%d.0s", m_card_num);
    m_capi_card = snap_card_alloc_dev (m_device, SNAP_VENDOR_ID_IBM, SNAP_DEVICE_ID_SNAP);

    if (NULL == m_capi_card) {
        /*ereport (ERROR,
                 (errcode (ERRCODE_INVALID_PARAMETER_VALUE),
                  errmsg ("Cannot allocate CARD!"))); */
	printf ("Cannot allocate CARD!\n");
        return -1;
    }

    printf ("Start to get action.\n");
    m_capi_action = get_action (m_capi_card, m_attach_flags, 5 * m_timeout);
    printf ("Finish get action.\n");

    return 0;

    /*
    m_capi_card = m_context->dn;
    m_capi_action = m_context->act;
    m_attach_flags = m_context->attach_flags;

    return 0;
    */
}

void HardwareManager::reg_write (uint32_t in_addr, uint32_t in_data, int in_eng_id)
{
    action_write (m_capi_card, REG(in_addr, in_eng_id), in_data);
    return;
}

uint32_t HardwareManager::reg_read (uint32_t in_addr, int in_eng_id)
{
    return action_read (m_capi_card, REG(in_addr, in_eng_id));
}

void HardwareManager::cleanup()
{
    //soft_reset (m_capi_card);

    snap_detach_action (m_capi_action);
    snap_card_free (m_capi_card);

    //if (m_context) {
      //  pfree (m_context);
    //}

    printf ("Deattach the card.\n");
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

/*
CAPIContext* HardwareManager::get_context()
{
    return m_context;
}
*/

void HardwareManager::reset_engine (int in_eng_id)
{
    soft_reset (m_capi_card, in_eng_id);
}

CAPICard* HardwareManager::get_capi_card()
{
    return m_capi_card;
}

int HardwareManager::get_timeout()
{
    return m_timeout;
}


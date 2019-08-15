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

#ifndef JobDirtest_H_h
#define JobDirtest_H_h

#include <iostream>
#include "JobBase.h"
#include "WorkerDirtest.h"
#include "ThreadDirtest.h"

class JobDirtest : public JobBase
{
public:
    enum eStatus {
        DONE = 0,
        FAIL,
        NUM_STATUS
    };

    // Constructor of the job base
    JobDirtest();
    JobDirtest (int in_id, int in_thread_id);
    JobDirtest (int in_id, int in_thread_id, HardwareManagerPtr in_hw_mgr);
    JobDirtest (int in_id, int in_thread_id, HardwareManagerPtr in_hw_mgr, bool in_debug);

    // Destructor of the job base
    ~JobDirtest();

    // Run this job
    virtual int run();

    // Set pointer to worker
    void set_worker (WorkerDirtestPtr in_worker);

    // Get pointer to worker
    WorkerDirtestPtr get_worker();

    // Initialize the job
    virtual int init();

    // Prepare the packet buffer
    int packet();

    // Perform the regex scan
    int scan();

    // Set packet buffer address for the job
    int set_packet_buffer (void* in_pkt_src_base, size_t in_max_alloc_pkt_size);

    // Set result buffer address for the job
    int set_result_buffer (void* in_stat_dest_base, size_t in_stat_size);

    // Get number of matched packets from this job
    size_t get_num_matched_pkt();

    // Get time used for buffer preparing (memcpy)
    uint64_t get_buff_prep_time();

    // Get time used for regex scanning
    uint64_t get_scan_time();

    // Release the buffers of the job
    void release_buffer();

    // Cleanup allocated memories
    virtual void cleanup();

private:
    // Pointer to the worker
    WorkerDirtestPtr m_worker;

    // Number of matched packets in this job
    size_t m_num_matched_pkt;

    // Pattern buffer base address for this job
    void* m_patt_src_base;

    // Pattern size
    size_t m_patt_size;

    // Packet buffer base address for this job
    void* m_pkt_src_base;

    // Packet size for this job
    size_t m_pkt_size;

    // Maximum allocated packet size of the buffer
    size_t m_max_alloc_pkt_size;

    // Result buffer base address for this job
    void* m_stat_dest_base;

    // Result buffer size
    size_t m_stat_size;

    // Time used for buffer preparing (memcpy)
    uint64_t m_buff_prep_time;

    // Time used for regex scanning
    uint64_t m_scan_time;
};

typedef boost::shared_ptr<JobDirtest> JobDirtestPtr;

#endif


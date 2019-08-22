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

#ifndef WORKERDIRTEST_H_h
#define WORKERDIRTEST_H_h

#include <iostream>
#include "WorkerBase.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "db_direct.h"
#ifdef __cplusplus
}
#endif

class WorkerDirtest : public WorkerBase
{
public:
    // Constructor of the worker base
    WorkerDirtest (HardwareManagerPtr in_hw_mgr, bool in_debug);

    // Destructor of the worker base
    ~WorkerDirtest();

    // Initialize each thread in this worker
    virtual int init();

    // Check if all threads have done their job
    virtual void check_thread_done();

    // Check if everything is ready for start threads
    virtual int check_start();

    // Set if we are going to use interrupt or polling
    void set_mode (bool in_interrupt);

    // Copy pattern buffer to worker
    void set_patt_src_base (void* in_patt_src_base, size_t in_patt_size);

    // Allocate packet buffers
    void set_pkt_src_base (void** in_job_pkt_src_bases,
                           size_t* in_job_pkt_sizes,
                           int in_num_job_per_thd,
                           int in_pkt_file_line_count);

    // Get the pattern buffer pointer
    void* get_pattern_buffer();

    // Get the packet buffer pointer
    void* get_packet_buffer (int in_job_id, int in_thread_id);

    // Get the size of the pattern buffer
    size_t get_pattern_buffer_size();

    // Get the size of the packet buffer
    size_t get_packet_buffer_size (int in_job_id, int in_thread_id);

    // Get the maximum lines to be processed by one job
    int get_max_line_count();

    // Check the results of each thread
    int check_results();

    // Get performance data from threads
    void get_thread_perf_data (uint64_t* max_buff_prep_time,
                               uint64_t* max_scan_time,
                               float* sd_buff_prep_time,
                               float* sd_scan_time);

    // Clean up any threads created for this worker
    virtual void cleanup();

private:
    // Use interrupt or poll to check thread done?
    bool m_interrupt;

    // Pointer to regex pattern buffer
    void* m_patt_src_base;

    // Size of the regex pattern buffer
    size_t m_patt_size;

    // Pointers to regex packet buffers for each job
    void** m_job_pkt_src_bases;

    // Sizes of the regex packet buffers for each job
    size_t* m_job_pkt_sizes;

    // Number of jobs for each thread
    int m_num_job_per_thd;

    // Line count of the packet file
    int m_pkt_file_line_count;
};

typedef boost::shared_ptr<WorkerDirtest> WorkerDirtestPtr;

#endif



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
#ifndef THREADDIRTEST_H_h
#define THREADDIRTEST_H_h

#include <iostream>
#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include "ThreadBase.h"
#include "WorkerDirtest.h"
#include "HardwareManager.h"

class ThreadDirtest : public ThreadBase
{
public:
    // Constructor of thread regex
    ThreadDirtest();
    ThreadDirtest (int in_id);
    ThreadDirtest (int in_id, int in_timeout);

    // Destructor of thread regex
    ~ThreadDirtest();

    // Initialize each jobs in this thread
    virtual int init();

    // Set pointer to worker
    void set_worker (WorkerDirtestPtr in_worker);

    // Set buffers base and number of blocks
    //void set_line_info (int in_base, int in_num);

    // Get the number of buffers (blocks) for different job
    //int get_num_lines_per_job (int in_job_id, int* out_start_blk_id);

    // Allocate the reused packet buffer and result buffer for this thread
    int allocate_buffers();

    // Work with the jobs
    virtual void work_with_job (JobPtr in_job);

    // Compare result with software
    int result();

    /*
    // Get the actual packet size of this thread
    size_t get_thread_pkt_size();

    // Get total time used for preparing pattern and packet buffers of all jobs
    uint64_t get_thread_buff_prep_time();

    // Get total regex matching runtime of all jobs
    uint64_t get_thread_runtime();
    */

    // Calculate the band_width of this thread
    float get_thread_band_width();

    // Cleanup
    virtual void cleanup();

    // Store the result packet IDs into the thread's result list(vector)
    int harvest_result_from_job (JobPtr in_job);

private:
    // Base address of worker's buffers for this thread
    //int m_buffers_base;

    // Total number of lines in file
    //int m_file_line_count;

    // Packet buffer base address for this thread
    void* m_pkt_src_base;
    
    // Maximum allocated size of the packet buffer
    size_t m_max_alloc_pkt_size;

    // Result buffer base address for this thread
    void* m_stat_dest_base;

    // Size of the output buffer
    size_t m_stat_size;

    // Total number of matched packets
    size_t m_num_matched_pkt;

    // Total time used for preparing pattern and packet buffers of all jobs
    //uint64_t m_buff_prep_time;
    
    // Total regex matching runtime of all jobs
    uint64_t m_runtime;

    // Pointer to worker
    WorkerDirtestPtr m_worker;

    // Vector that stores results from all jobs
    std::vector<uint32_t> m_result_id;
};

typedef boost::shared_ptr<ThreadDirtest> ThreadDirtestPtr;

#endif



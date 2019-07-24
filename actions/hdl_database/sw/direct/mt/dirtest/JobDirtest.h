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

    // Get the result
    int result();

    // Cleanup allocated memories
    virtual void cleanup();

    // Allocate packet buffer and stat buffer
    //int allocate_packet_buffer ();

    // Set the job descriptor
    //void set_job_desc (CAPIRegexJobDescriptor* in_job_desc);

    // Set result compare if it checks offset
    void set_no_chk_offset (int in_no_chk_offset);

    // Set pkt_src_base for the job descripter
    void set_pkt_src_base (void* in_pkt_src_base, size_t in_pkt_size);

    // Set stat_dest_base for the job descripter
    void set_stat_dest_base (size_t in_stat_size);

private:
    // Pointer to worker for adding job descriptors
    WorkerDirtestPtr m_worker;

    // The Job descritpor which contains all information for a regex job
    //CAPIRegexJobDescriptor* m_job_desc;


    size_t m_num_matched_pkt;

    int m_no_chk_offset;

    void* m_patt_src_base;

    size_t m_patt_size;

    void* m_pkt_src_base;

    size_t m_pkt_size;

    void* m_stat_dest_base;

    size_t m_stat_size;


    // Internal functions to handle relation buffers
    /* 
    //void* capi_regex_pkt_psql_internal (Relation rel,
    int capi_regex_pkt_psql_internal ( //Relation rel,
                                        //int attr_id,
                                        int start_blk_id,
                                        int num_blks,
                                        void* pkt_src_base,
                                        size_t* size,
                                        size_t* size_wo_hw_hdr,
                                        size_t* num_pkt,
                                        int64_t* t_pkt_cpy);

    // Handle the packet preparation
    int capi_regex_pkt_psql (CAPIRegexJobDescriptor* job_desc //,
                              Relation rel, int attr_id );
    

    // Aligned variant of palloc0
    void* aligned_palloc0 (size_t in_size);

    // An array to hold all allocated pointers,
    // need this array to remember which pointer needs to be freed.
    std::vector<void*> m_allocated_ptrs;
    */

};

typedef boost::shared_ptr<JobDirtest> JobDirtestPtr;

#endif

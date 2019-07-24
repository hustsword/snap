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

#include "boost/make_shared.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/chrono.hpp"
#include "HardwareManager.h"
#include "WorkerDirtest.h"
#include "ThreadDirtest.h"
#include "JobDirtest.h"
#include "Interface.h"

using namespace boost::chrono;

int start_regex_workers (int num_engines, const char* patt_file_path, int no_chk_offset, void* pkt_src_base, size_t pkt_size, size_t stat_size)
{
    printf ("Running on regex worker\n");

    /*
    if (NULL == in_capiss) {
        elog (ERROR, "Invalid CAPI Scan State pointer");
        return -1;
    }
    */

    HardwareManagerPtr hw_mgr =  boost::make_shared<HardwareManager> (0, 0, 1000);

    WorkerDirtestPtr worker = boost::make_shared<WorkerDirtest> (hw_mgr, false);
    worker->set_mode (false);

    printf ("Init hardware\n");
    ERROR_CHECK (hw_mgr->init());
    printf ("Compile pattern\n");
    ERROR_CHECK (worker->regex_compile (patt_file_path));

    printf ("Create %d job(s) for this worker\n", num_engines);

    /*
    ThreadDirtestPtr thd = boost::make_shared<ThreadDirtest> (0,1000);
    JobDirtestPtr job = boost::make_shared<JobDirtest> (0,0,hw_mgr,false);
    CAPIRegexJobDescriptor* job_desc = (CAPIRegexJobDescriptor*) palloc0 (sizeof (CAPIRegexJobDescriptor));
    job->set_job_desc (job_desc);
    job->set_no_chk_offset (no_chk_offset);
    job->set_pkt_src_base (pkt_src_base);
    job->set_stat_size (stat_size);
    job->set_worker (worker);
    thd->add_job(job);
    worker->add_thread(thd);
    */

    // Create threads
    for (int i = 0; i < num_engines; i++) {
        ThreadDirtestPtr thd = boost::make_shared<ThreadDirtest> (i, 1000);

        // Create 1 job for each thread
        JobDirtestPtr job = boost::make_shared<JobDirtest> (0, i, hw_mgr, false);
        //CAPIRegexJobDescriptor* job_desc = (CAPIRegexJobDescriptor*) palloc0 (sizeof (CAPIRegexJobDescriptor));
        //job->set_job_desc (job_desc);
        job->set_no_chk_offset (no_chk_offset);
        job->set_pkt_src_base (pkt_src_base, pkt_size);
        job->set_stat_dest_base (stat_size);
        job->set_worker (worker);

        thd->add_job (job);

        // Add thread to worker
        worker->add_thread (thd);
    }

    printf ("Finish setting up jobs.\n");

    do {
        // Read relation buffers
        //high_resolution_clock::time_point t_start = high_resolution_clock::now();
        //worker->read_buffers();
        //if (worker->init()) {
            //elog (ERROR, "Failed to initialize worker");
            //return -1;
        //}
        high_resolution_clock::time_point t_end0 = high_resolution_clock::now();
        //auto duration0 = duration_cast<microseconds> (t_end0 - t_start).count();
        // Start work, multithreading starts from here
        worker->start();
        // Multithreading ends at here
        high_resolution_clock::time_point t_end1 = high_resolution_clock::now();
        auto duration1 = duration_cast<microseconds> (t_end1 - t_end0).count();
        // Cleanup objects created for this procedure
        hw_mgr->cleanup();
        worker->cleanup();
        high_resolution_clock::time_point t_end2 = high_resolution_clock::now();
        auto duration2 = duration_cast<microseconds> (t_end2 - t_end1).count();

        //elog (INFO, "Read buffers finished after %lu microseconds (us)\n", (uint64_t) duration0);
        printf ("Work finished after %lu microseconds (us)\n", (uint64_t) duration1);
        printf ("Cleanup finished after %lu microseconds (us)\n", (uint64_t) duration2);

        printf ("Worker done!\n");
    } while (0);

    return 0;

fail:
    return -1;
}


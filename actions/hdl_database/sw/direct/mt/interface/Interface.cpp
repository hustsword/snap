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

int start_regex_workers (int num_engines, 
                         int num_job_per_thd,
                         void* patt_src_base,
                         size_t patt_size,
			 size_t pkt_size,
			 void** job_pkt_src_bases,
			 size_t* job_pkt_sizes,
			 int file_line_count,
                         struct snap_card* dn,
                         struct snap_action* act,
                         snap_action_flag_t attach_flags,
                         float* thd_scan_bw,
                         float* wkr_bw,
			 float* total_bw,
                         uint64_t* max_buff,
			 float* sd_buff,
			 uint64_t* max_scan,
			 float* sd_scan,
                         uint64_t* cleanup_time)
{
    //printf ("Running on regex worker\n");

    HardwareManagerPtr hw_mgr =  boost::make_shared<HardwareManager> (0, dn, act, attach_flags, 0, 1000);

    WorkerDirtestPtr worker = boost::make_shared<WorkerDirtest> (hw_mgr, false);
    worker->set_mode (false);

    ERROR_CHECK (hw_mgr->init());

    //printf ("Set buffers to worker...\n");
    worker->set_patt_src_base (patt_src_base, patt_size);
    worker->set_pkt_src_base (job_pkt_src_bases, job_pkt_sizes, num_job_per_thd, file_line_count);
    //printf ("Finish setting buffers\n");

    //printf ("Create %d thread(s) for this worker\n", num_engines);
    //printf ("Create %d job(s) for each thread\n", num_job_per_thd);

    // Create threads
    for (int i = 0; i < num_engines; i++) {
        ThreadDirtestPtr thd = boost::make_shared<ThreadDirtest> (i, 1000);
        thd->set_worker (worker);

        for (int j = 0; j < num_job_per_thd; j++) {
            JobDirtestPtr job = boost::make_shared<JobDirtest> (j, i, hw_mgr, false);
            job->set_worker (worker);
            thd->add_job (job);
        }

        // Add thread to worker
        worker->add_thread (thd);
    }

    //printf ("Finish setting up jobs.\n");

    do {
        uint64_t start_time, elapsed_time;

	if (worker->init()) {
            printf ("ERROR: Failed to initialize worker\n");
            return -1;
        }

        start_time = get_usec();

        // Start work, multithreading starts from here
        worker->start();
        // Multithreading ends at here
	
        elapsed_time = get_usec() - start_time;
	uint64_t worker_runtime = elapsed_time;
	*wkr_bw = perf_calc (worker_runtime, (uint64_t)pkt_size);

	worker->get_thread_perf_data (max_buff, max_scan, sd_buff, sd_scan);
	*thd_scan_bw = perf_calc (*max_scan, (uint64_t)pkt_size);

	ERROR_CHECK (worker->check_results());

        start_time = get_usec();
        // Cleanup objects created for this procedure
        hw_mgr->cleanup();
        worker->cleanup();
        elapsed_time = get_usec() - start_time;
        *cleanup_time = elapsed_time;
	*total_bw = perf_calc (worker_runtime + *cleanup_time, (uint64_t)pkt_size);

        printf ("Work finished after %lu usec (%0.3f MB/sec). ", worker_runtime, *wkr_bw);

        printf ("Cleanup finished after %lu microseconds (us)\n", *cleanup_time);

        //printf ("Worker done!\n");
    } while (0);

    return 0;

fail:
    return -1;
}


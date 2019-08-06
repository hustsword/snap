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
#include "WorkerRegex.h"
#include "ThreadRegex.h"
#include "JobRegex.h"
#include "Interface.h"

using namespace boost::chrono;

int start_regex_workers (PGCAPIScanState* in_capiss)
{
    elog (DEBUG1, "Running on regex worker");

    if (NULL == in_capiss) {
        elog (ERROR, "Invalid CAPI Scan State pointer");
        return -1;
    }

    HardwareManagerPtr hw_mgr = boost::make_shared<HardwareManager> (0, 0, 1000);

    elog (DEBUG1, "Init hardware");
    ERROR_CHECK (hw_mgr->init());

    if (in_capiss->capi_regex_num_engines != hw_mgr->get_num_engines()) {
        elog (ERROR, "Number of engines set %d is not equal to actual number of engines %d",
              in_capiss->capi_regex_num_engines, hw_mgr->get_num_engines());
    }
    elog (INFO, "Total %d job(s) for this worker", in_capiss->capi_regex_num_jobs);
    elog (INFO, "Create %d thread(s) for this worker", in_capiss->capi_regex_num_threads);


    Buffer* buffers = NULL;
    int num_blks = 0;
    size_t num_tups = 0;
    read_buffers (&buffers, in_capiss->css.ss.ss_currentRelation, &num_blks, &num_tups);

    int num_engines = in_capiss->capi_regex_num_engines;
    int num_blks_each = num_blks / num_engines;
    int num_blks_last = num_blks_each + num_blks % num_blks_each;
    int num_tups_each = num_tups / num_engines;
    int start_blk_for_worker = 0;
 
    elog (INFO, "Total %d job(s) for all workers", in_capiss->capi_regex_num_jobs);
    elog (INFO, "Create %d thread(s) for all workers", in_capiss->capi_regex_num_threads);
    
    int num_thds_each = in_capiss->capi_regex_num_threads / num_engines;
    int start_thd_for_worker = 0;
    int num_jobs_each = in_capiss->capi_regex_num_jobs / num_engines;
    int start_job_for_worker = 0;

    std::vector<WorkerRegexPtr> workers;

    //TODO: the number of jobs and threads must be able to be divided by the number of engines
    //      the number of jobs must be able to be divided by the number of threads

    // Create workers
    for (int h = 0; h < in_capiss->capi_regex_num_engines; h++) {
        int num_blks_this = num_blks_each;
        int num_tups_this = num_tups_each;
        if (h == in_capiss->capi_regex_num_engines - 1) {
            num_blks_this = num_blks_last;
        }
        WorkerRegexPtr worker = boost::make_shared<WorkerRegex> (hw_mgr,
                            in_capiss->css.ss.ss_currentRelation,
                            in_capiss->capi_regex_attr_id,
                            false,
                            h,
                            num_blks_this,
                            num_tups_this);

        worker->set_mode (false);
        worker->set_buffers (buffers + start_blk_for_worker);

        elog (DEBUG1, "Compile pattern");
        ERROR_CHECK (worker->regex_compile (in_capiss->capi_regex_pattern));

        // Create threads
        for (int i = 0; i < num_thds_each; i++) {
            ThreadRegexPtr thd = boost::make_shared<ThreadRegex> (start_thd_for_worker + i, 1000);
            thd->set_worker (worker);

            // Assign jobs to each thread
            int job_start_id = start_job_for_worker + (num_jobs_each / num_thds_each) * i;
            int num_jobs_in_thd = num_jobs_each / num_thds_each;

            // if (i != 0 && i == in_capiss->capi_regex_num_threads - 1) {
            //     num_jobs_in_thd += in_capiss->capi_regex_num_jobs % num_jobs_in_thd;
            // }

            for (int j = 0; j < num_jobs_in_thd; j++) {
                // Create 1 job
                JobRegexPtr job = boost::make_shared<JobRegex> (job_start_id + j, start_thd_for_worker + i, hw_mgr, false);
                job->set_job_desc (in_capiss->capi_regex_job_descs[job_start_id + j]);
                capi_regex_job_init (in_capiss->capi_regex_job_descs[job_start_id + j], hw_mgr->get_context());
                job->set_worker (worker);
                job->set_thread (thd);
                thd->add_job (job);
            }

            // Add thread to worker
            worker->add_thread (thd);
        }

        workers.push_back(worker);
        start_thd_for_worker += num_thds_each;
        start_job_for_worker += num_jobs_each;
        start_blk_for_worker += num_blks_this;
    }


    elog (DEBUG1, "Finish setting up jobs.");

    do {
        // Read relation buffers
        high_resolution_clock::time_point t_start = high_resolution_clock::now();
        //worker->read_buffers();

        for (auto worker_p = workers.begin(); worker_p != workers.end(); worker_p++) {
            if ((*worker_p)->init()) {
                elog (ERROR, "Failed to initialize worker");
                return -1;
            }
        }
        

        high_resolution_clock::time_point t_end0 = high_resolution_clock::now();
        auto duration0 = duration_cast<microseconds> (t_end0 - t_start).count();

        // Start work, multithreading starts from here
        for (auto worker_p = workers.begin(); worker_p != workers.end(); worker_p++) {
            (*worker_p)->start();
        }
        // Multithreading ends at here

        high_resolution_clock::time_point t_end1 = high_resolution_clock::now();
        auto duration1 = duration_cast<microseconds> (t_end1 - t_end0).count();

        // Cleanup objects created for this procedure
        hw_mgr->cleanup();

        high_resolution_clock::time_point t_end1_5 = high_resolution_clock::now();
        for (auto worker_p = workers.begin(); worker_p != workers.end(); worker_p++) {
            (*worker_p)->cleanup();
        }

        release_buffers(buffers, num_blks);

        high_resolution_clock::time_point t_end2 = high_resolution_clock::now();
        auto duration1_5 = duration_cast<microseconds> (t_end2 - t_end1_5).count();
        auto duration2 = duration_cast<microseconds> (t_end2 - t_end1).count();

        elog (INFO, "Read buffers finished after %lu microseconds (us)", (uint64_t) duration0);
        elog (INFO, "Work finished after %lu microseconds (us)", (uint64_t) duration1);
        elog (INFO, "Worker Cleanup finished after %lu microseconds (us)", (uint64_t) duration1_5);
        elog (INFO, "Cleanup finished after %lu microseconds (us)", (uint64_t) duration2);

        elog (DEBUG1, "Worker done!");
    } while (0);

    return 0;

fail:
    return -1;
}


void read_buffers(Buffer&* buffers, Relation relation, int & num_blks, size_t & num_tups)
{
    capi_regex_check_relation (relation);

    *num_blks = RelationGetNumberOfBlocksInFork (relation, MAIN_FORKNUM);
    int tups = 0;

    *buffers = (Buffer*) palloc0 (sizeof (Buffer) * num_blks);

    for (int blk_num = 0; blk_num < num_blks; ++blk_num) {
        Buffer buf = ReadBufferExtended (relation, MAIN_FORKNUM, blk_num, RBM_NORMAL, NULL);

        Page page = (Page) BufferGetPage (buf);
        int num_lines = PageGetMaxOffsetNumber (page);
        tups += num_lines;

        (*buffers)[blk_num] = buf;
    }

    *num_tups = tups;

    elog (INFO, "Read %d buffers from relation", *num_blks);
    elog (INFO, "Read %zu tuples from relation", *num_tups);
}

void release_buffers(Buffer* buffers, int num_blks)
{
    if (buffers) {
        for (int blk_num = 0; blk_num < num_blks; ++blk_num) {
            Buffer buf = buffers[blk_num];
            ReleaseBuffer (buf);
        }

        pfree (buffers);
    }
}


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

#include <malloc.h>
#include "ThreadDirtest.h"
#include "JobDirtest.h"

ThreadDirtest::ThreadDirtest()
    : ThreadBase (0, 600),
      m_buffers_base (0),
      m_file_line_count (0),
      m_pkt_src_base (NULL),
      m_max_alloc_pkt_size (0),
      m_stat_dest_base (NULL),
      m_stat_size (0),
      m_num_matched_pkt (0),
      m_buff_prep_time (0),
      m_runtime (0),
      m_worker (NULL)
{
    //printf("create dirtest thread\n");
}

ThreadDirtest::ThreadDirtest (int in_id)
    : ThreadBase (in_id),
      m_buffers_base (0),
      m_file_line_count (0),
      m_pkt_src_base (NULL),
      m_max_alloc_pkt_size (0),
      m_stat_dest_base (NULL),
      m_stat_size (0),
      m_num_matched_pkt (0),
      m_buff_prep_time (0),
      m_runtime (0),
      m_worker (NULL)
{
    //printf("create dirtest thread on engine %d\n", in_id);
}

ThreadDirtest::ThreadDirtest (int in_id, int in_timeout)
    : ThreadBase (in_id, in_timeout),
      m_buffers_base (0),
      m_file_line_count (0),
      m_pkt_src_base (NULL),
      m_max_alloc_pkt_size (0),
      m_stat_dest_base (NULL),
      m_stat_size (0),
      m_num_matched_pkt (0),
      m_buff_prep_time (0),
      m_runtime (0),
      m_worker (NULL)
{
    //printf("create dirtest thread on engine %d\n", in_id);
}

ThreadDirtest::~ThreadDirtest()
{
}

int ThreadDirtest::init()
{
    //printf("Eng %d: init thread\n", m_id);
    //for (size_t i = 0; i < m_jobs.size(); i++) {
      //  if (m_jobs[i]->init()) {
        //    return -1;
       // }
    //}

    int thd_start_line_id = 0;
    int thd_num_lines = m_worker->get_line_count();

    set_line_info (thd_start_line_id, thd_num_lines);
    allocate_buffers();

    return 0;
}

void ThreadDirtest::set_worker (WorkerDirtestPtr in_worker)
{
    m_worker = in_worker;
}

void ThreadDirtest::set_line_info (int in_base, int in_file_line_count)
{
    m_buffers_base = in_base;
    m_file_line_count = in_file_line_count;
}

int ThreadDirtest::get_num_lines_per_job (int in_job_id, int* out_start_line_id)
{
    int num_jobs = m_jobs.size();
    int num_lines_per_job = 0;

    if (m_file_line_count <= 0) {
        return -1;
    }

    if (num_jobs <= 0) {
        return -1;
    }

    int lines_per_job = m_file_line_count / num_jobs;
    int lines_last_job = m_file_line_count % num_jobs;

    if (0 == lines_per_job) {
        // TODO: if number of total blocks is less than number of jobs, get panic.
        // Need to revisit this behavior.
        return -1;
    }

    *out_start_line_id = m_buffers_base + in_job_id * lines_per_job;

    num_lines_per_job = lines_per_job;

    if (in_job_id == (num_jobs - 1)) {
        num_lines_per_job += lines_last_job;
    }

    return num_lines_per_job;
}

int ThreadDirtest::allocate_buffers()
{
    int num_jobs = m_jobs.size();

    if (m_file_line_count <= 0) {
        return -1;
    }

    if (num_jobs <= 0) {
        return -1;
    }

    // TODO: is there a way to know exactly how many tuples we have before iterating all buffers?
    //uint64_t total_row_count = m_worker->get_num_tuples_per_thread (m_id);
    //uint64_t row_count = total_row_count / num_jobs + 1;
    int max_lines_per_job = m_file_line_count / num_jobs + m_file_line_count % num_jobs;

    // Allocate the packet buffer
    // The max size that should be alloc
    // TODO: assume maximum size in packet buffer for tuples is 2048 bytes
    size_t line_size = 2048 + 64;
    size_t max_alloc_size = (max_lines_per_job < 4096 ? 4096 : max_lines_per_job) * line_size;
    m_pkt_src_base = alloc_mem (64, max_alloc_size);
    m_max_alloc_pkt_size = max_alloc_size;

    if (m_pkt_src_base == NULL) {
        printf ("ERROR: Failed to allocate packet buffer for thread %d\n", m_id);
        return -1;
    }

    // Allocate the result buffer
    // TODO: Estimate the result buffer by the presumption: num_pkt == num_tuples
    size_t real_stat_size = (OUTPUT_STAT_WIDTH / 8) * max_lines_per_job * 2;
    size_t stat_size = (real_stat_size % 4096 == 0) ? real_stat_size : real_stat_size + (4096 - (real_stat_size % 4096));

    // At least 4K for output buffer.
    if (stat_size == 0) {
        stat_size = 4096;
    }

    m_stat_dest_base = alloc_mem (64, stat_size);
    m_stat_size = stat_size;

    if (NULL == m_stat_dest_base) {
        printf ("ERROR: Unable to allocate stat buffer!\n");
        return -1;
    }

    return 0;
}

void ThreadDirtest::work_with_job (JobPtr in_job)
{
    //printf("Eng %d: thread work with job\n", m_id);
    JobDirtestPtr job = boost::dynamic_pointer_cast<JobDirtest> (in_job);

    if (NULL == job) {
        printf ("ERROR: Failed to get pointer to JobDirtest\n");
        return;
    }

    do {
	//if (m_pkt_src_base == NULL) printf("pkt_src_base is NULL before setting packet buffer to job\n");
        if (job->set_packet_buffer (m_pkt_src_base, m_max_alloc_pkt_size)) {
            printf ("ERROR: Failed to set packet buffer for job %d\n", job->get_id());
            return;
        }

        if (job->set_result_buffer (m_stat_dest_base, m_stat_size)) {
            printf ("ERROR: Failed to set result buffer for job %d\n", job->get_id());
            return;
        }
    } while (0);

    //uint64_t start_time, elapsed_time;
    //start_time = get_usec();

    do {
        if (job->run()) {
            printf ("ERROR: Failed to run the JobDirtest\n");
            return;
        }
    } while (0);

    //printf ("Eng %d getting time breakdowns from Job %d\n", m_id, job->get_id());
    m_buff_prep_time += job->get_buff_prep_time();
    m_runtime += job->get_job_runtime();
    //printf ("Eng %d finished getting time breakdown from Job %d\n", m_id, job->get_id());

    //printf ("Eng %d harvesting results from Job %d\n", m_id, job->get_id());
    do {
        if (harvest_result_from_job (job)) {
            printf ("ERROR: Failed to harvest result\n");
            return;
        }
    } while (0);
    //printf ("Eng %d finished harvesting results from Job %d\n", m_id, job->get_id());

    //elapsed_time = get_usec() - start_time;
    //m_runtime += elapsed_time;

    return;
}

int ThreadDirtest::result()
{
    int rc = 0;

    //printf ("Eng %d: compare number of matched packets with software\n", m_id);
    if (compare_num_matched_pkt (m_num_matched_pkt)) {
        rc = 1;
    }

    //printf ("Eng %d: start comparing result IDs with software\n", m_id);
    for (size_t i = 0; i < m_result_id.size(); i++) {
        if (compare_result_id (m_result_id[i])) {
            rc = 1;
        }
    }
    //printf ("Eng %d: finished comparing result IDs with rc = %d\n", m_id, rc);

    return rc;
}

uint64_t ThreadDirtest::get_thread_buff_prep_time()
{
    return m_buff_prep_time;
}

uint64_t ThreadDirtest::get_thread_runtime()
{
    return m_runtime;
}

float ThreadDirtest::get_thread_band_width()
{
    size_t total_pkt_size = m_file_line_count * (2048 + 64);
    float thread_band_width = print_time (m_runtime, total_pkt_size);
    return thread_band_width;
    /*
    float job_total_band_width = 0;
    for (size_t i = 0; i < m_jobs.size(); i++) {
        job_total_band_width += boost::dynamic_pointer_cast<JobDirtest> (m_jobs[i]) -> get_job_band_width();
    }
    return job_total_band_width;
    */
}

void ThreadDirtest::cleanup()
{
    //printf("Eng %d: clean up thread\n", m_id);
    free_mem (m_pkt_src_base);
    free_mem (m_stat_dest_base);

    for (size_t i = 0; i < m_jobs.size(); i++) {
        m_jobs[i]->cleanup();
    }

    m_jobs.clear();
    m_result_id.clear();
}

int ThreadDirtest::harvest_result_from_job (JobPtr in_job)
{
    int i = 0, j = 0;
    uint32_t pkt_id = 0;
    JobDirtestPtr job = boost::dynamic_pointer_cast<JobDirtest> (in_job);
    size_t job_num_matched_pkt = job->get_num_matched_pkt();

    //printf ("Eng %d: start reading from m_stat_dest_base written by Job %d\n", m_id, job->get_id());
    for (i = 0; i < (int)job_num_matched_pkt; i++) {
        for (j = 4; j < 8; j++) {
            pkt_id |= (((uint8_t*)m_stat_dest_base)[i * 10 + j] << (j % 4) * 8);
        }

        m_result_id.push_back(pkt_id);

        pkt_id = 0;
    }
    //printf ("Eng %d: finished reading from m_stat_dest_base with %zu matched packets\n", m_id, job_num_matched_pkt);

    m_num_matched_pkt += job_num_matched_pkt;

    return 0;
}


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

#include <boost/chrono.hpp>
#include "WorkerDirtest.h"
#include "ThreadDirtest.h"
#include "constants.h"

WorkerDirtest::WorkerDirtest (HardwareManagerPtr in_hw_mgr, bool in_debug)
    : WorkerBase (in_hw_mgr),
      m_interrupt (true),
      m_patt_src_base (NULL),
      m_patt_size (0),
      m_job_pkt_src_bases (NULL),
      m_job_pkt_sizes (NULL),
      m_num_job_per_thd (0),
      m_pkt_file_line_count (0)
{
    //printf("create dirtest worker\n");
    m_job_manager_en = false;
}

WorkerDirtest::~WorkerDirtest()
{
}

void WorkerDirtest::set_mode (bool in_interrupt)
{
    m_interrupt = in_interrupt;
}

int WorkerDirtest::init()
{
    //printf("init worker\n");
    for (size_t i = 0; i < m_threads.size(); i++) {
        if (m_threads[i]->init()) {
            return -1;
        }
    }

    return 0;
}

void WorkerDirtest::check_thread_done()
{
    //printf("worker checking thread done\n");
    if (m_interrupt) {
        printf ("Interrupt mode is not supported yet!");
    } else {
        do {
            bool all_done = true;

            for (int i = 0; i < (int)m_threads.size(); i++) {
                if (0 == m_threads[i]->get_num_remaining_jobs()) {
                    m_threads[i]->stop();
                } else {
                    all_done = false;
                }
            }

            if (all_done) {
                printf ("Worker -- All jobs done");
                break;
            }

            boost::this_thread::interruption_point();
        } while (1);
    }
}

int WorkerDirtest::check_start()
{
    //printf("worker check start\n");
    if (NULL == m_patt_src_base || 0 == m_patt_size) {
        printf ("ERROR: Invalid pattern buffer");
        return -1;
    }

    return 0;
}

void WorkerDirtest::set_patt_src_base (void* in_patt_src_base, size_t in_patt_size)
{
    m_patt_size = in_patt_size;
    m_patt_src_base = alloc_mem (64, m_patt_size);
    memcpy (m_patt_src_base, in_patt_src_base, m_patt_size);
}

void WorkerDirtest::set_pkt_src_base (void** in_job_pkt_src_bases, 
		                      size_t* in_job_pkt_sizes, 
				      int in_num_job_per_thd,
				      int in_pkt_file_line_count)
{
    m_job_pkt_src_bases = in_job_pkt_src_bases;
    m_job_pkt_sizes = in_job_pkt_sizes;
    m_num_job_per_thd = in_num_job_per_thd;
    m_pkt_file_line_count = in_pkt_file_line_count;
}

void* WorkerDirtest::get_pattern_buffer()
{
    return m_patt_src_base;
}

void* WorkerDirtest::get_packet_buffer (int in_job_id, int in_thread_id)
{
    return m_job_pkt_src_bases[in_thread_id * m_num_job_per_thd + in_job_id];
}

size_t WorkerDirtest::get_pattern_buffer_size()
{
    return m_patt_size;
}

size_t WorkerDirtest::get_packet_buffer_size (int in_job_id, int in_thread_id)
{
    return m_job_pkt_sizes[in_thread_id * m_num_job_per_thd + in_job_id];
}

int WorkerDirtest::get_max_line_count()
{
    int total_num_jobs = m_num_job_per_thd * m_threads.size();
    int max_lines_per_job = m_pkt_file_line_count / total_num_jobs + m_pkt_file_line_count % total_num_jobs;
    return max_lines_per_job;
}


int WorkerDirtest::check_results()
{
    int rc = 0;
    int total_num_matched_pkt = 0;

    for (size_t i = 0; i < m_threads.size(); i++) {
	total_num_matched_pkt += boost::dynamic_pointer_cast<ThreadDirtest> (m_threads[i]) -> get_thread_num_matched_pkt();
    }

    if (compare_num_matched_pkt (total_num_matched_pkt)) {
	rc = 1;
    }

    return rc;
}

void WorkerDirtest::get_thread_perf_data (uint64_t* max_buff_prep_time, 
					  uint64_t* max_scan_time,
		                          float* sd_buff_prep_time, 
					  float* sd_scan_time)
{
    uint64_t buff_prep_time, scan_time;
    uint64_t sum_buff_prep_time = 0, sum_scan_time = 0;
    float avg_buff_prep_time, avg_scan_time;
    float sum_bp_dev = 0, sum_sc_dev = 0;

    *max_buff_prep_time = 0;
    *max_scan_time = 0;

    for (size_t i = 0; i < m_threads.size(); i++) {
        buff_prep_time = boost::dynamic_pointer_cast<ThreadDirtest> (m_threads[i]) -> get_thread_buff_prep_time();
	scan_time = boost::dynamic_pointer_cast<ThreadDirtest> (m_threads[i]) -> get_thread_scan_time();

	if (buff_prep_time > *max_buff_prep_time) {
	    *max_buff_prep_time = buff_prep_time;
	}
	if (scan_time > *max_scan_time) {
	    *max_scan_time = scan_time;
	}

	sum_buff_prep_time += buff_prep_time;
	sum_scan_time += scan_time;
    }

    // Caculate E[X]
    avg_buff_prep_time = (float)sum_buff_prep_time / m_threads.size();
    avg_scan_time = (float)sum_scan_time / m_threads.size();

    // Add (X-E[X])^2 for each X
    for (size_t i = 0; i < m_threads.size(); i++) {
	buff_prep_time = boost::dynamic_pointer_cast<ThreadDirtest> (m_threads[i]) -> get_thread_buff_prep_time();
	scan_time = boost::dynamic_pointer_cast<ThreadDirtest> (m_threads[i]) -> get_thread_scan_time();

	sum_bp_dev += ((float)buff_prep_time - avg_buff_prep_time) * ((float)buff_prep_time - avg_buff_prep_time);
	sum_sc_dev += ((float)scan_time - avg_scan_time) * ((float)scan_time - avg_scan_time);
    }
    
    // SD(X) = sqrt(Var(X)) = sqrt(E[(X-E[X])^2])
    *sd_buff_prep_time = sqrtf (sum_bp_dev / m_threads.size());
    *sd_scan_time = sqrtf (sum_sc_dev / m_threads.size());
}

void WorkerDirtest::cleanup()
{
    //printf("clean up worker\n");
    for (size_t i = 0; i < m_threads.size(); i++) {
        m_threads[i]->cleanup();
    }

    m_threads.clear();
    m_job_pkt_src_bases = NULL;
    m_job_pkt_sizes = NULL;

    free_mem (m_patt_src_base);
}


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
      m_pkt_src_base (NULL),
      m_pkt_size (0),
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

void WorkerDirtest::set_pkt_src_base (const char* in_pkt_file_path, int num_job_per_thread)
{
    FILE* fp = fopen (in_pkt_file_path, "r");
    char* line = NULL;
    size_t len = 0;
    ssize_t read;
    int lines_read = 0;
    int curr_job_id = 0;

    if (fp == NULL) {
        printf ("ERROR: PACKET file not existed %s\n", in_pkt_file_path);
        return;
    }

    for (int i = 0; i < num_job_per_thread; i++) {
	job_pkt_src_bases.push_back(NULL);
	job_pkt_sizes.push_back(0);
    }

    m_pkt_file_line_count = get_file_line_count (fp);
    fclose (fp);

    size_t alloc_size = (m_pkt_file_line_count < 4096 ? 4096 : m_pkt_file_line_count) * (2048 + 64);
    m_pkt_src_base = alloc_mem (64, alloc_size);

    fp = fopen (in_pkt_file_path, "r");

    void* pkt_src = m_pkt_src_base;

    job_pkt_src_bases[0] = m_pkt_src_base;
    
    while ((read = getline (&line, &len, fp)) != -1) {
	if (curr_job_id != num_job_per_thread - 1 && 
	    lines_read == (curr_job_id + 1) * (m_pkt_file_line_count / num_job_per_thread)) {
	    job_pkt_sizes[curr_job_id] = (unsigned char*) pkt_src - (unsigned char*) job_pkt_src_bases[curr_job_id];
            curr_job_id++;
	    job_pkt_src_bases[curr_job_id] = pkt_src;
	}

        remove_newline (line);
        read--;
        //printf ("PACKET line read with length %zu :\n", read);
        //printf ("%s\n", line);
        // packet ID starts from 1
        pkt_src = fill_one_packet (line, read, pkt_src, lines_read + 1);
        //printf ("PACKET Source Address 0X%016lX\n", (uint64_t)pkt_src);
	
        lines_read++;
    }

    job_pkt_sizes[curr_job_id] = (unsigned char*)pkt_src - (unsigned char*) job_pkt_src_bases[curr_job_id];

    //printf ("---------- Packet Buffer: %p\n", m_pkt_src_base);

    /*
    if (verbose_level > 2) {
        __hexdump (stdout, pkt_src_base, (pkt_src - m_pkt_src_base));
    }
    */

    fclose (fp);

    if (line) {
        free (line);
    }

    m_pkt_size = (unsigned char*)pkt_src - (unsigned char*)m_pkt_src_base;
}

void* WorkerDirtest::get_pattern_buffer()
{
    return m_patt_src_base;
}

void* WorkerDirtest::get_packet_buffer (int in_job_id)
{
    if (in_job_id >= (int)job_pkt_src_bases.size()) {
	printf ("ERROR: invalid job ID, cannot get packet buffer address\n");
	return NULL;
    }
    return job_pkt_src_bases[in_job_id];
}

size_t WorkerDirtest::get_pattern_buffer_size()
{
    return m_patt_size;
}

size_t WorkerDirtest::get_packet_buffer_size (int in_job_id)
{
    if (in_job_id >= (int)job_pkt_sizes.size()) {
	printf ("ERROR: invalid job ID, cannot get packet buffer size\n");
	return -1;
    }
    return job_pkt_sizes[in_job_id];
}

int WorkerDirtest::get_line_count()
{
    return m_pkt_file_line_count;
}


int WorkerDirtest::check_results()
{
    int rc = 0;
    for (size_t i = 0; i < m_threads.size(); i++) {
        if (boost::dynamic_pointer_cast<ThreadDirtest> (m_threads[i]) -> result()) {
            rc = 1;
        }
    }
    return rc;
}

size_t WorkerDirtest::get_worker_pkt_size()
{
    return m_pkt_size;
}


float WorkerDirtest::get_sum_band_width()
{
    float sum_band_width = 0;
    for (size_t i = 0; i < m_threads.size(); i++) {
        sum_band_width += boost::dynamic_pointer_cast<ThreadDirtest> (m_threads[i]) -> get_thread_band_width();
    }
    //printf ("Thread total band width is %0.3f MB/sec.\n", sum_band_width);
    return sum_band_width;
}

void WorkerDirtest::cleanup()
{
    //printf("clean up worker\n");
    for (size_t i = 0; i < m_threads.size(); i++) {
        m_threads[i]->cleanup();
    }

    m_threads.clear();
    job_pkt_src_bases.clear();
    job_pkt_sizes.clear();

    free_mem (m_patt_src_base);
    free_mem (m_pkt_src_base);
}


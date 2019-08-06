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
#include "time.h"
#include "JobDirtest.h"
#include "constants.h"

JobDirtest::JobDirtest()
    : JobBase(),
      m_worker (NULL),
      //m_thread (NULL),
      m_num_matched_pkt (0),
      m_patt_src_base (NULL),
      m_patt_size (0),
      m_pkt_src_base (NULL),
      m_pkt_size (0),
      m_max_alloc_pkt_size (0),
      m_stat_dest_base (NULL),
      m_stat_size (0)//,
      //m_buff_prep_time (0),
      //m_runtime (0)
{
    //printf ("create new dirtest job\n");
}

JobDirtest::JobDirtest (int in_id, int in_thread_id)
    : JobBase (in_id, in_thread_id),
      m_worker (NULL),
      //m_thread (NULL),
      m_num_matched_pkt (0),
      m_patt_src_base (NULL),
      m_patt_size (0),
      m_pkt_src_base (NULL),
      m_pkt_size (0),
      m_max_alloc_pkt_size (0),
      m_stat_dest_base (NULL),
      m_stat_size (0)//,
      //m_buff_prep_time (0),
      //m_runtime (0)
{
    //printf("create new dirtest job on engine %d\n", in_thread_id);
}

JobDirtest::JobDirtest (int in_id, int in_thread_id, HardwareManagerPtr in_hw_mgr)
    : JobBase (in_id, in_thread_id, in_hw_mgr),
      m_worker (NULL),
      //m_thread (NULL),
      m_num_matched_pkt (0),
      m_patt_src_base (NULL),
      m_patt_size (0),
      m_pkt_src_base (NULL),
      m_pkt_size (0),
      m_max_alloc_pkt_size (0),
      m_stat_dest_base (NULL),
      m_stat_size (0)//,
      //m_buff_prep_time (0),
      //m_runtime (0)
{
    //printf("create new dirtest job on engine %d\n", in_thread_id);
}

JobDirtest::JobDirtest (int in_id, int in_thread_id, HardwareManagerPtr in_hw_mgr, bool in_debug)
    : JobBase (in_id, in_thread_id, in_hw_mgr, in_debug),
      m_worker (NULL),
      //m_thread (NULL),
      m_num_matched_pkt (0),
      m_patt_src_base (NULL),
      m_patt_size (0),
      m_pkt_src_base (NULL),
      m_pkt_size (0),
      m_max_alloc_pkt_size (0),
      m_stat_dest_base (NULL),
      m_stat_size (0)//,
      //m_buff_prep_time (0),
      //m_runtime (0)
{
    //printf("create new dirtest job on engine %d\n", in_thread_id);
}

JobDirtest::~JobDirtest()
{
}

int JobDirtest::run()
{
    //uint64_t start_time, elapsed_time;

    //uint64_t t0 = get_usec();

    do {
        if (init()) {
            printf ("ERROR: Failed to perform regex job initializing\n");
            fail();
            return -1;
        }

        if (packet()) {
            printf ("ERROR: Failed to perform regex packet preparing\n");
            fail();
            return -1;
        }
    } while (0);

    //uint64_t t1 = get_usec();
    //printf ("Eng %d Job %d: finished buffer preparing after %lu usec\n", m_thread_id, m_id, t1 - t0);

    //start_time = get_usec();

    do {
        // TODO: Only 1 job is allowed to access hardware at a time.
        //boost::lock_guard<boost::mutex> lock (ThreadBase::m_global_mutex);

        if (scan()) {
            printf ("ERROR: Failed to perform regex scanning\n");
            fail();
            return -1;
        }
    } while (0);

    //uint64_t t2 = get_usec();
    //printf ("Eng %d Job %d: finished scanning after %lu usec\n", m_thread_id, m_id, t2 - t1);

    done();

    //printf ("Eng %d Job %d finished with size %zu\n", m_thread_id, m_id, m_pkt_size);

    return 0;
}

void JobDirtest::set_worker (WorkerDirtestPtr in_worker)
{
    m_worker = in_worker;
}

/*
void JobDirtest::set_thread (ThreadDirtestPtr in_thread)
{
    m_thread = in_thread;
}
*/

WorkerDirtestPtr JobDirtest::get_worker()
{
    return m_worker;
}

int JobDirtest::init()
{
    //printf("Eng %d: init job %d\n", m_thread_id, m_id);
    if (NULL == m_worker) {
        printf ("ERROR: Worker points to NULL, cannot perform regex job init\n");
        return -1;
    }

    /*
    if (NULL == m_thread) {
        printf ("ERROR: Thread points to NULL, cannot perform regex job init\n");
        return -1;
    } */

    if (NULL == m_hw_mgr) {
        printf ("ERROR: Hardware manager points to NULL, cannot perform regex job init\n");
        return -1;
    }

    // Copy the pattern from worker to job
    m_patt_src_base = m_worker->get_pattern_buffer();
    m_patt_size = m_worker->get_pattern_buffer_size();
   
    // Reset the engine
    m_hw_mgr->reset_engine (m_thread_id);

    return 0;
}

int JobDirtest::packet()
{
    //printf("Eng %d Job %d: prepare packet\n", m_thread_id, m_id);
    if (NULL == m_worker) {
        printf ("ERROR: Worker points to NULL, cannot perform regex packet preparation\n");
        return -1;
    }

    if (NULL == m_pkt_src_base) {
        printf ("ERROR: pkt_src_base is NULL\n");
        return -1;
    }
    if (NULL == m_stat_dest_base) {
        printf ("ERROR: stat_dest_base is NULL\n");
        return -1;
    }

    //uint64_t t0 = get_usec();

    m_pkt_size = m_worker->get_packet_buffer_size (m_id);
    memcpy (m_pkt_src_base, m_worker->get_packet_buffer (m_id), m_pkt_size);
    //printf ("Eng %d Job %d: packet size is %zu\n", m_thread_id, m_id, m_pkt_size);

    //uint64_t t1 = get_usec();
    //printf ("Eng %d Job %d: finished memcpy after %lu usec\n", m_thread_id, m_id, t1 - t0);

    /*
    if (fetch_pkt_from_file()) {
        printf ("ERROR: Fail to prepare packet buffer\n");
        return -1;
    }
    */

    return 0;
}

int JobDirtest::scan()
{
    //printf("Eng %d Job %d: scanning...\n", m_thread_id, m_id);
    if (regex_scan (m_hw_mgr->get_capi_card(),
                    ACTION_WAIT_TIME,
                    m_patt_src_base,
                    m_pkt_src_base,
                    m_stat_dest_base,
                    &m_num_matched_pkt,
                    m_patt_size,
                    m_pkt_size,
                    m_stat_size,
                    m_thread_id)) {
        printf ("ERROR: Failed to scan the table\n");
        return -1;
    }

    //printf ("Eng %d Job %d: finish regex_scan with %d matched packets.\n", m_thread_id, m_id, (int)m_num_matched_pkt);

    int count = 0;
    do {
        //printf ("Eng %d: draining %i! \n", m_thread_id, count);
        m_hw_mgr->reg_read(ACTION_STATUS_L, m_thread_id);
        count++;
    } while (count < 10);

    uint32_t reg_data = m_hw_mgr->reg_read(ACTION_STATUS_H, m_thread_id);
    //printf ("Eng %d Job %d: After draining, number of matched packets: %d\n", m_thread_id, m_id, reg_data);
    m_num_matched_pkt = reg_data;
    //printf ("Eng %d Job %d: Set m_num_matched_pkt to %d\n", m_thread_id, m_id, (int)m_num_matched_pkt);

    return 0;
}

/*
int JobDirtest::result()
{
    //printf("Eng %d: compare result\n", m_thread_id);
    //print_results ((m_pkt_size/1024) *2, m_stat_dest_base);
    if (compare_results (m_num_matched_pkt, m_stat_dest_base, m_no_chk_offset)) {
        printf ("ERROR: Miscompare detected between hardware and software ref model on Engine %d.\n", m_thread_id);
        return -1;
    } else {
    //printf ("Test PASSED for Engine %d!\n", m_thread_id);
    }
    return 0;
}
*/

int JobDirtest::set_packet_buffer (void* in_pkt_src_base, size_t in_max_alloc_pkt_size)
{
    m_pkt_src_base = in_pkt_src_base;

    if (NULL == m_pkt_src_base) {
        printf ("ERROR: packet buffer assigned: fail\n");
        return -1;
    }

    m_max_alloc_pkt_size = in_max_alloc_pkt_size;
    return 0;
}

int JobDirtest::set_result_buffer (void* in_stat_dest_base, size_t in_stat_size)
{
    m_stat_dest_base = in_stat_dest_base;
    m_stat_size = in_stat_size;
    return 0;
}

size_t JobDirtest::get_num_matched_pkt()
{
    return m_num_matched_pkt;
}

/*
size_t JobDirtest::get_pkt_size()
{
    return m_pkt_size;
}

uint64_t JobDirtest::get_buff_prep_time()
{
    return m_buff_prep_time;
}

uint64_t JobDirtest::get_job_runtime()
{
    return m_runtime;
}
*/

void JobDirtest::cleanup()
{
    //printf("Eng %d: clean up job %d\n", m_thread_id, m_id);
    //free_mem (m_pkt_src_base);
    //free_mem (m_stat_dest_base);
    m_hw_mgr = NULL;
    m_worker = NULL;
    //m_thread = NULL;
}

/*
int JobDirtest::fetch_pkt_from_file()
{
    FILE* fp = fopen (m_worker->get_pkt_file_path(), "r");
    int start_line_id = 0;
    char* line = NULL;
    size_t len = 0;
    ssize_t read;
    int lines_read = 0;
    void* pkt_src = m_pkt_src_base;

    if (fp == NULL) {
        printf ("ERROR: PACKET file not existed %s\n", m_worker->get_pkt_file_path());
        return -1;
    }

    int num_pkt_in_job = m_thread->get_num_lines_per_job (m_id, &start_line_id);

    while ((read = getline (&line, &len, fp)) != -1 && lines_read < start_line_id + num_pkt_in_job) {
        remove_newline (line);
        read--;
        //printf ("PACKET line read with length %zu :\n", read);
        //printf ("%s\n", line);
        if (lines_read >= start_line_id) {
            pkt_src = fill_one_packet (line, read, pkt_src, lines_read + 1);
            //printf ("PACKET ID: %d, PACKET Source Address 0X%016lX\n", lines_read, (uint64_t)pkt_src);
        }
	lines_read++;
    }

    //printf ("Total size of packet buffer used: %ld\n", (uint64_t) (pkt_src - m_pkt_src_base));

    //printf ("---------- Packet Buffer: %p\n", m_pkt_src_base);

    
    if (verbose_level > 2) {
        __hexdump (stdout, pkt_src_base, (pkt_src - m_pkt_src_base));
    }
    

    fclose (fp);

    if (line) {
        free (line);
    }

    m_pkt_size = (unsigned char*)pkt_src - (unsigned char*)m_pkt_src_base;

    return 0;
}
*/

/*
void JobDirtest::set_no_chk_offset (int in_no_chk_offset)
{
    m_no_chk_offset = in_no_chk_offset;
}
   
void JobDirtest::set_pkt_src_base (void* in_pkt_src_base, size_t in_pkt_size)
{
    //printf ("Eng %d: copying packet to job\n", m_thread_id);
    m_pkt_size = in_pkt_size;
    m_pkt_src_base = alloc_mem (64, m_pkt_size);
    memcpy (m_pkt_src_base, in_pkt_src_base, m_pkt_size);
}

void JobDirtest::set_stat_dest_base (size_t in_stat_size)
{
    //printf ("Eng %d: setting dest buffer\n", m_thread_id);
    m_stat_size = in_stat_size;
    m_stat_dest_base = alloc_mem (64, m_stat_size);
    memset (m_stat_dest_base, 0, m_stat_size);
}

float JobDirtest::get_job_band_width()
{
    return m_band_width;
}
*/


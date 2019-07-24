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
      m_num_matched_pkt (0),
      m_no_chk_offset (0),
      m_patt_src_base (NULL),
      m_patt_size (0),
      m_pkt_src_base (NULL),
      m_pkt_size (0),
      m_stat_dest_base (NULL),
      m_stat_size (0)
{
    printf ("create new dirtest job\n");
}

JobDirtest::JobDirtest (int in_id, int in_thread_id)
    : JobBase (in_id, in_thread_id),
      m_worker (NULL),
      m_num_matched_pkt (0),
      m_no_chk_offset (0),
      m_patt_src_base (NULL),
      m_patt_size (0),
      m_pkt_src_base (NULL),
      m_pkt_size (0),
      m_stat_dest_base (NULL),
      m_stat_size (0)
{
    printf("create new dirtest job\n");
}

JobDirtest::JobDirtest (int in_id, int in_thread_id, HardwareManagerPtr in_hw_mgr)
    : JobBase (in_id, in_thread_id, in_hw_mgr),
      m_worker (NULL),
      m_num_matched_pkt (0),
      m_no_chk_offset (0),
      m_patt_src_base (NULL),
      m_patt_size (0),
      m_pkt_src_base (NULL),
      m_pkt_size (0),
      m_stat_dest_base (NULL),
      m_stat_size (0)
{
    printf("create new dirtest job\n");
}

JobDirtest::JobDirtest (int in_id, int in_thread_id, HardwareManagerPtr in_hw_mgr, bool in_debug)
    : JobBase (in_id, in_thread_id, in_hw_mgr, in_debug),
      m_worker (NULL),
      m_num_matched_pkt (0),
      m_no_chk_offset (0),
      m_patt_src_base (NULL),
      m_patt_size (0),
      m_pkt_src_base (NULL),
      m_pkt_size (0),
      m_stat_dest_base (NULL),
      m_stat_size (0)
{
    printf("create new dirtest job\n");
}

JobDirtest::~JobDirtest()
{
}

int JobDirtest::run()
{
    /*
    if (NULL == m_job_desc) {
        elog (ERROR, "Job descriptor is NULL");
        fail();
        return -1;
    }
    */

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

    do {
        // TODO: Only 1 job is allowed to access hardware at a time.
        //boost::lock_guard<boost::mutex> lock (ThreadBase::m_global_mutex);

        if (scan()) {
            printf ("ERROR: Failed to perform regex scanning\n");
            fail();
            return -1;
        }
    } while (0);

    if (result()) {
        printf ("ERROR: Failed to perform regex packet result harvesting\n");
        fail();
        return -1;
    }

    done();

    return 0;
}

void JobDirtest::set_worker (WorkerDirtestPtr in_worker)
{
    m_worker = in_worker;
}

WorkerDirtestPtr JobDirtest::get_worker()
{
    return m_worker;
}

int JobDirtest::init()
{
    printf("init job");
    if (NULL == m_worker) {
        printf ("ERROR: Worker points to NULL, cannot perform regex job init\n");
        return -1;
    }

    if (NULL == m_hw_mgr) {
        printf ("ERROR: Hardware manager points to NULL, cannot perform regex job init\n");
        return -1;
    }
    
    /*
    if (capi_regex_job_init (m_job_desc, m_hw_mgr->get_context())) {
        elog (ERROR, "Failed to initialize the job descriptor");
        return -1;
    }
    */

    // Copy the pattern from worker to job
    m_patt_src_base = m_worker->get_pattern_buffer();
    m_patt_size = m_worker->get_pattern_buffer_size();

    //int start_blk_id = 0;
    //int num_blks = m_worker->get_num_blks_per_thread (m_thread_id, &start_blk_id); 

    // Get the blocks for this job
    //m_job_desc->num_blks = num_blks;
    //m_job_desc->start_blk_id = start_blk_id;

    /*
    if (0 == num_blks) {
        elog (ERROR, "Number of blocks is invalid for thread %d job %d",
              m_thread_id, m_id);
        return -1;
    }
    */

    // Allocate packet buffer
    /*
    if (allocate_packet_buffer()) {
        elog (ERROR, "Failed to allocate packet buffer");
        return -1;
    }
    */

    // Assign the thread id to this job descriptor
    //m_job_desc->thread_id = m_thread_id;

    // Reset the engine
    m_hw_mgr->reset_engine (m_thread_id);

    return 0;
}

int JobDirtest::packet()
{
    printf("prepare packet");
    if (NULL == m_worker) {
        printf ("ERROR: Worker points to NULL, cannot perform regex packet preparation\n");
        return -1;
    }

    //TODO
    /*
    if (capi_regex_pkt_psql (m_job_desc, m_worker->get_relation(),
                             m_worker->get_attr_id())) {
        elog (ERROR, "Failed to prepare packet buffer");
        return -1;
    }
    
    if (NULL == m_job_desc) {
        elog (ERROR, "Job descriptor is NULL, cannot perform regex packet preparation");
        return -1;
    }
    */

    if (NULL == m_pkt_src_base) {
        printf ("ERROR: pkt_src_base is NULL\n");
        return -1;
    }
    if (NULL == m_stat_dest_base) {
        printf ("ERROR: stat_dest_base is NULL\n");
        return -1;
    }

    return 0;
}

int JobDirtest::scan()
{
    printf("scanning...\n");
    if (regex_scan (m_hw_mgr->get_capi_card(),
                    m_hw_mgr->get_timeout(),
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

    return 0;
}

int JobDirtest::result()
{
    printf("compare result\n");
    if (compare_results (m_num_matched_pkt, m_stat_dest_base, m_no_chk_offset)) {
        printf ("ERROR: Miscompare detected between hardware and software ref model on Engine %d.\n", m_thread_id);
        //elog (ERROR, "Failed to get the result of regex scan");
        return -1;
    }
    return 0;
}

/*
void JobDirtest::set_job_desc (CAPIRegexJobDescriptor* in_job_desc)
{
    m_job_desc = in_job_desc;
}
*/

void JobDirtest::cleanup()
{
    printf("clean up job\n");
    free_mem (m_pkt_src_base);
    free_mem (m_stat_dest_base);
    /*
    for (size_t i = 0; i < m_allocated_ptrs.size(); i++) {
        pfree (m_allocated_ptrs[i]);
    }
    */
}

/*
int JobDirtest::allocate_packet_buffer()
{
    if (m_job_desc == NULL) {
        return -1;
    }

    // TODO: is there a way to know exactly how many tuples we have before iterating all buffers?
    // uint64_t row_count = m_worker->get_num_tuples_per_thread (m_thread_id);

    // The max size that should be alloc
    // TODO: assume maximum size in packet buffer for tuples is 2048 bytes
    size_t row_size = 2048 + 64;
    size_t max_alloc_size = (row_count < MIN_NUM_PKT ? MIN_NUM_PKT : row_count) * row_size;

    m_job_desc->pkt_src_base = alloc_mem (64, max_alloc_size);
    //m_job_desc->pkt_src_base = aligned_palloc0 (max_alloc_size);
    m_job_desc->max_alloc_pkt_size = max_alloc_size;
    //pkt_src = pkt_src_base;

    if (m_job_desc->pkt_src_base == NULL) {
        elog (ERROR, "Failed to allocate packet buffer for thread %d job %d", m_thread_id, m_id);
        return -1;
    }

    return 0;
}
*/

void JobDirtest::set_no_chk_offset (int in_no_chk_offset)
{
    m_no_chk_offset = in_no_chk_offset;
}
   
void JobDirtest::set_pkt_src_base (void* in_pkt_src_base, size_t in_pkt_size)
{
    printf ("copying packet to job\n");
    m_pkt_size = in_pkt_size;
    m_pkt_src_base = alloc_mem (64, m_pkt_size);
    memcpy (m_pkt_src_base, in_pkt_src_base, m_pkt_size);
}

void JobDirtest::set_stat_dest_base (size_t in_stat_size)
{
    printf ("setting dest buffer\n");
    m_stat_size = in_stat_size;
    m_stat_dest_base = alloc_mem (64, m_stat_size);
    memset (m_stat_dest_base, 0, m_stat_size);
}


/*
//void* JobRegex::capi_regex_pkt_psql_internal (Relation rel, int attr_id,
int JobDirtest::capi_regex_pkt_psql_internal ( Relation rel, int attr_id, 
        int start_blk_id, int num_blks,
        void* pkt_src_base,
        size_t* size, size_t* size_wo_hw_hdr,
        size_t* num_pkt, int64_t* t_pkt_cpy)
{
    if (NULL == pkt_src_base) {
        return -1;
    }

    //void* pkt_src_base = NULL;
    //void* pkt_src      = NULL;
    void* pkt_src      = pkt_src_base;
    TupleDesc tupdesc  = RelationGetDescr (rel);

    //// TODO: is there a way to know exactly how many tuples we have before iterating all buffers?
    //uint64_t row_count = m_worker->get_num_tuples_per_thread (m_thread_id);

    //// The max size that should be alloc
    //// TODO: assume maximum size in packet buffer for tuples is 2048 bytes
    //size_t row_size = 2048 + 64;
    //size_t max_alloc_size = (row_count < MIN_NUM_PKT ? MIN_NUM_PKT : row_count) * row_size;

    ////pkt_src_base = alloc_mem (64, max_alloc_size);
    //pkt_src_base = aligned_palloc0 (max_alloc_size);
    //pkt_src = pkt_src_base;

    //if (NULL == pkt_src_base) {
    //    elog (ERROR, "Failed to allocate packet buffer for size: %zu", max_alloc_size);
    //}

    Buffer buf = InvalidBuffer;
    int first_blk_num_lines = 0;

    for (int blk_num = start_blk_id; blk_num < num_blks + start_blk_id; ++blk_num) {
        do {
            buf = m_worker->m_buffers[blk_num];
        } while (0);

        Page page = (Page) BufferGetPage (buf);
        int num_lines = PageGetMaxOffsetNumber (page);

        if (blk_num == start_blk_id) {
            first_blk_num_lines = num_lines;
        }

        for (int line_num = 0; line_num <= num_lines; ++line_num) {
            ItemId id = PageGetItemId (page, line_num);
            uint16 lp_offset = ItemIdGetOffset (id);
            uint16 lp_len = ItemIdGetLength (id);
            HeapTupleHeader tuphdr = (HeapTupleHeader) PageGetItem (page, id);

            // TODO: be careful about the packet id.
            // The packet id is supposed to be the row ID in the relation,
            // is this really the correct way to do this?
            int pkt_id = line_num + blk_num * first_blk_num_lines;

            if (ItemIdHasStorage (id) &&
                lp_len >= MinHeapTupleSize &&
                lp_offset == MAXALIGN (lp_offset)) {

                int attr_len = 0;
                bytea* attr_ptr = DatumGetByteaP (get_attr (tuphdr, tupdesc, lp_len, attr_id, &attr_len));
                attr_len = VARSIZE (attr_ptr) - VARHDRSZ;

                (*size_wo_hw_hdr) += attr_len;
                pkt_src = fill_one_packet (VARDATA (attr_ptr), attr_len, pkt_src, pkt_id);
                (*num_pkt)++;
            }
        }
    }

    (*size) = (uint64_t) pkt_src - (uint64_t) pkt_src_base;

    //return pkt_src_base;
    return 0;
}



int JobDirtest::capi_regex_pkt_psql (CAPIRegexJobDescriptor* job_desc,  Relation rel, int attr_id )
{
    if (NULL == job_desc) {
        return -1;
    }

    if (NULL == job_desc->pkt_src_base) {
        elog (ERROR, "Invalid packet buffer");
        return -1;
    }

    int real_stat_size;
    int stat_size;

    //
    //job_desc->pkt_src_base = capi_regex_pkt_psql_internal (rel,
    if (capi_regex_pkt_psql_internal (rel,
                                      attr_id,
                                      job_desc->start_blk_id,
                                      job_desc->num_blks,
                                      job_desc->pkt_src_base,
                                      & (job_desc->pkt_size),
                                      & (job_desc->pkt_size_wo_hw_hdr),
                                      & (job_desc->num_pkt),
                                      & (job_desc->t_regex_pkt_copy))) {
        elog (ERROR, "Failed to run capi_regex_pkt_psql_internal");
        return -1;
    }

    if (job_desc->pkt_size > job_desc->max_alloc_pkt_size) {
        elog (ERROR, "In packet preparation, the real number of packet buffer (%zu)"
              " is larger than estimated (%zu)", job_desc->pkt_size, job_desc->max_alloc_pkt_size);
        return -1;
    }

    // Allocate the result buffer per the number of packets in the packet buffer
    // TODO: To reserve twice more spaces in case hardware goes into panic (i.e., writing to more spaces than expected)
    // TODO: hardware issue?
    real_stat_size = (OUTPUT_STAT_WIDTH / 8) * (job_desc->num_pkt) * 2;
    stat_size = (real_stat_size % 4096 == 0) ? real_stat_size : real_stat_size + (4096 - (real_stat_size % 4096));

    // At least 4K for output buffer.
    if (stat_size == 0) {
        stat_size = 4096;
    }

    job_desc->stat_dest_base = alloc_mem (64, stat_size);
    //job_desc->stat_dest_base = aligned_palloc0 (stat_size);
    job_desc->stat_size = stat_size;

    if (NULL == job_desc->stat_dest_base) {
        elog (ERROR, "Unable to allocate stat buffer!");
        return -1;
    }

    if (job_desc->pkt_size == 0) {
        elog (ERROR, "Packet size is zero, something wrong!");
        return -1;
    }

    return 0;
}

void* JobDirtest::aligned_palloc0 (size_t in_size)
{
    // TODO: Performance of using postgresql's memory allocation version is not good for CAPI,
    //       Need to figure out a better way of handling memory allocation/deallocation.
    //       For now, use posix_memalign instead.
    // 4096 bytes aligned allocation
    //void* ptr = palloc0 (in_size + 4096);
    void* ptr = MemoryContextAllocHuge (CurrentMemoryContext, (in_size + 4096));
    MemSetAligned (ptr, 0, in_size + 4096);
    void* aligned_ptr = (void*) (((uint64_t)ptr + 4096) & 0xFFFFFFFFFFFFF000ULL);
    m_allocated_ptrs.push_back (ptr);

    return aligned_ptr;
}
*/


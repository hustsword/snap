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
#include "JobRegex.h"
#include "constants.h"

JobRegex::JobRegex()
    : JobBase(),
      m_worker (NULL),
      m_job_desc (NULL)
{
}

JobRegex::JobRegex (int in_id, int in_thread_id)
    : JobBase (in_id, in_thread_id),
      m_worker (NULL),
      m_job_desc (NULL)
{
}

JobRegex::JobRegex (int in_id, int in_thread_id, HardwareManagerPtr in_hw_mgr)
    : JobBase (in_id, in_thread_id, in_hw_mgr),
      m_worker (NULL),
      m_job_desc (NULL)
{
}

JobRegex::JobRegex (int in_id, int in_thread_id, HardwareManagerPtr in_hw_mgr, bool in_debug)
    : JobBase (in_id, in_thread_id, in_hw_mgr, in_debug),
      m_worker (NULL),
      m_job_desc (NULL)
{
}

JobRegex::~JobRegex()
{
}

int JobRegex::run()
{
    if (NULL == m_job_desc) {
        elog (ERROR, "Job descriptor is NULL");
        fail();
        return -1;
    }

    do {
        if (init()) {
            elog (ERROR, "Failed to perform regex job initializing");
            fail();
            return -1;
        }

        if (packet()) {
            elog (ERROR, "Failed to perform regex packet preparing");
            fail();
            return -1;
        }
    } while (0);

    do {
        // TODO: Only 1 job is allowed to access hardware at a time.
        //boost::lock_guard<boost::mutex> lock (ThreadBase::m_global_mutex);

        if (scan()) {
            elog (ERROR, "Failed to perform regex scanning");
            fail();
            return -1;
        }
    } while (0);

    if (result()) {
        elog (ERROR, "Failed to perform regex packet result harvesting");
        fail();
        return -1;
    }

    done();

    return 0;
}

void JobRegex::set_worker (WorkerRegexPtr in_worker)
{
    m_worker = in_worker;
}

void JobRegex::set_thread (ThreadRegexPtr in_thread)
{
    m_thread = in_thread;
}

WorkerRegexPtr JobRegex::get_worker()
{
    return m_worker;
}

int JobRegex::init()
{
    if (NULL == m_worker) {
        elog (ERROR, "Worker points to NULL, cannot perform regex job init");
        return -1;
    }

    if (NULL == m_hw_mgr) {
        elog (ERROR, "Hardware manager points to NULL, cannot perform regex job init");
        return -1;
    }

    // Copy the pattern from worker to job
    m_job_desc->patt_src_base = m_worker->get_pattern_buffer();
    m_job_desc->patt_size = m_worker->get_pattern_buffer_size();
    int start_blk_id = 0;
    int num_blks = m_thread->get_num_blks_per_job (m_id, &start_blk_id);
    // Get the blocks for this job
    m_job_desc->num_blks = num_blks;
    m_job_desc->start_blk_id = start_blk_id;

    if (0 == num_blks) {
        elog (ERROR, "Number of blocks is invalid for thread %d job %d",
              m_thread_id, m_id);
        return -1;
    }

    // Assign the thread id to this job descriptor
    m_job_desc->thread_id = m_thread_id;

    // Reset the engine
    m_hw_mgr->reset_engine (m_thread_id);
    
    return 0;
}

int JobRegex::packet()
{
    if (NULL == m_worker) {
        elog (ERROR, "Worker points to NULL, cannot perform regex packet preparation");
        return -1;
    }

    if (capi_regex_pkt_psql (m_job_desc, m_worker->get_relation(),
                             m_worker->get_attr_id())) {
        elog (ERROR, "Failed to prepare packet buffer");
        return -1;
    }

    return 0;
}

int JobRegex::scan()
{
    if (capi_regex_scan (m_job_desc)) {
        elog (ERROR, "Failed to scan the table");
        return -1;
    }

    return 0;
}

int JobRegex::result()
{
    if (NULL == m_job_desc->stat_dest_base) {
        elog (ERROR, "Invalid result buffer");
        return -1;
    }

    if (capi_regex_result_harvest (m_job_desc)) {
        elog (ERROR, "Failed to get the result of regex scan");
        return -1;
    }

    return 0;
}

void JobRegex::set_job_desc (CAPIRegexJobDescriptor* in_job_desc)
{
    m_job_desc = in_job_desc;
}

int JobRegex::set_packet_buffer (void* in_pkt_src_base, size_t in_max_alloc_pkt_size)
{
    if (NULL == m_job_desc) {
        elog (ERROR, "Job descriptor is NULL");
        fail();
        return -1;
    }

    m_job_desc->pkt_src_base = in_pkt_src_base;

    if (NULL == m_job_desc->pkt_src_base) {
        elog (ERROR, "packet buffer assigned: fail");
        return -1;
    }

    m_job_desc->max_alloc_pkt_size = in_max_alloc_pkt_size;
    return 0;
}

int JobRegex::set_result_buffer (void* in_stat_dest_base, size_t in_stat_size)
{
    if (NULL == m_job_desc) {
        elog (ERROR, "Job descriptor is NULL");
        fail();
        return -1;
    }

    m_job_desc->stat_dest_base = in_stat_dest_base;
    m_job_desc->stat_size = in_stat_size;
    return 0;
}

// TODO: Should not be used
void JobRegex::cleanup()
{
    if (NULL == m_job_desc->stat_dest_base) {
        elog (ERROR, "free an already freed stat buffer!");
    }
}

int JobRegex::capi_regex_pkt_psql_internal (Relation rel, int attr_id,
        int start_blk_id, int num_blks,
        void* pkt_src_base,
        size_t* size, size_t* size_wo_hw_hdr,
        size_t* num_pkt, int64_t* t_pkt_cpy)
{
    if (NULL == pkt_src_base) {
        return -1;
    }

    void* pkt_src      = pkt_src_base;
    TupleDesc tupdesc  = RelationGetDescr (rel);

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
            HeapTupleHeader tuphdr = (HeapTupleHeader) PageGetItem (page, id); // also check ItemIdHasStorage (id)

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

int JobRegex::capi_regex_pkt_psql (CAPIRegexJobDescriptor* job_desc, Relation rel, int attr_id)
{
    if (NULL == job_desc) {
        return -1;
    }

    if (NULL == job_desc->pkt_src_base) {
        elog (ERROR, "Invalid packet buffer");
        return -1;
    }

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

    return 0;
}


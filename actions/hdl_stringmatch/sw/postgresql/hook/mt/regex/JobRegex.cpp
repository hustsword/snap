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
        boost::lock_guard<boost::mutex> lock (ThreadBase::m_global_mutex);

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

    if (capi_regex_job_init (m_job_desc, m_hw_mgr->get_context())) {
        elog (ERROR, "Failed to initialize the job descriptor");
        return -1;
    }

    // Copy the pattern from worker to job
    m_job_desc->patt_src_base = m_worker->get_pattern_buffer();
    m_job_desc->patt_size = m_worker->get_pattern_buffer_size();

    int start_blk_id = 0;
    int num_blks = 0;
    num_blks = m_worker->get_num_blks_per_thread (m_thread_id, &start_blk_id);

    // Get the blocks for this job
    m_job_desc->start_blk_id = start_blk_id;
    m_job_desc->num_blks = num_blks;

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

void* JobRegex::capi_regex_pkt_psql_internal (Relation rel, int attr_id,
        int start_blk_id, int num_blks,
        size_t* size, size_t* size_wo_hw_hdr,
        size_t* num_pkt, int64_t* t_pkt_cpy)
{
    void* pkt_src_base = NULL;
    void* pkt_src      = NULL;
    TupleDesc tupdesc  = RelationGetDescr (rel);

    uint64_t row_count = 512000ULL;

    // The max size that should be alloc
    size_t max_alloc_size = (row_count < MIN_NUM_PKT ? MIN_NUM_PKT : row_count) * (64 + 2048);

    pkt_src_base = alloc_mem (64, max_alloc_size);
    pkt_src = pkt_src_base;

    if (NULL == pkt_src_base) {
        elog (ERROR, "Failed to allocate packet buffer for size: %zu", max_alloc_size);
    }

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

    return pkt_src_base;
}

int JobRegex::capi_regex_pkt_psql (CAPIRegexJobDescriptor* job_desc, Relation rel, int attr_id)
{
    if (job_desc == NULL) {
        return -1;
    }

    int real_stat_size;
    int stat_size;

    job_desc->pkt_src_base = capi_regex_pkt_psql_internal (rel,
                             attr_id,
                             job_desc->start_blk_id,
                             job_desc->num_blks,
                             & (job_desc->pkt_size),
                             & (job_desc->pkt_size_wo_hw_hdr),
                             & (job_desc->num_pkt),
                             & (job_desc->t_regex_pkt_copy));

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
    job_desc->stat_size = stat_size;


    if (job_desc->pkt_size == 0 ||
        job_desc->pkt_src_base == NULL ||
        job_desc->stat_dest_base == NULL) {
        return -1;
    }

    return 0;
}


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


#include "pg_capi_internal.h"
#include "mt/interface/Interface.h"


void* fill_one_packet (const char* in_pkt, int size, void* in_pkt_addr, int in_pkt_id)
{
    unsigned char* pkt_base_addr = (unsigned char*) in_pkt_addr;
    int pkt_id = in_pkt_id;
    uint32_t bytes_used = 0;
    uint16_t pkt_len = size;

    if (((uint64_t)pkt_base_addr & 0x3FULL) != 0) {
        elog (INFO, "WARNING: Address %p is not 64B aligned", pkt_base_addr);
    }

    // The frame header
    for (int i = 0; i < 4; i++) {
        pkt_base_addr[bytes_used] = 0x5A;
        bytes_used ++;
    }

    // The frame size
    pkt_base_addr[bytes_used] = (pkt_len & 0xFF);
    bytes_used ++;
    pkt_base_addr[bytes_used] = 0;
    pkt_base_addr[bytes_used] |= ((pkt_len >> 8) & 0xF);
    bytes_used ++;

    memset (pkt_base_addr + bytes_used, 0, 54);
    bytes_used += 54;

    for (int i = 0; i < 4 ; i++) {
        pkt_base_addr[bytes_used] = ((pkt_id >> (8 * i)) & 0xFF);
        bytes_used++;
    }

    memcpy (pkt_base_addr + bytes_used, in_pkt, pkt_len);
    bytes_used += pkt_len;

    // Padding to 64 bytes alignment
    bytes_used--;

    do {
        if ((((uint64_t) (pkt_base_addr + bytes_used)) & 0x3F) == 0x3F) { //the last address of the packet stream is 512bit/64byte aligned
            break;
        } else {
            bytes_used ++;
            pkt_base_addr[bytes_used] = 0x00; //padding 8'h00 until the 512bit/64byte alignment
        }

    } while (1);

    bytes_used++;

    return pkt_base_addr + bytes_used;
}

void* fill_one_pattern (const char* in_patt, void* in_patt_addr, int in_patt_id)
{
    unsigned char* patt_base_addr = (unsigned char*) in_patt_addr;
    int config_len = 0;
    unsigned char config_bytes[PATTERN_WIDTH_BYTES];
    int x;
    uint32_t pattern_id = in_patt_id;
    uint16_t patt_byte_cnt;
    uint32_t bytes_used = 0;

    for (x = 0; x < PATTERN_WIDTH_BYTES; x++) {
        config_bytes[x] = 0;
    }

    elog (DEBUG1, "PATT[%d] %s\n", pattern_id, in_patt);

    fregex_get_config (in_patt,
                       MAX_TOKEN_NUM,
                       MAX_STATE_NUM,
                       MAX_CHAR_NUM,
                       MAX_CHAR_PER_TOKEN,
                       config_bytes,
                       &config_len,
                       0);

    for (int i = 0; i < 4; i++) {
        patt_base_addr[bytes_used] = 0x5A;
        bytes_used++;
    }

    patt_byte_cnt = (PATTERN_WIDTH_BYTES - 4);
    patt_base_addr[bytes_used] = patt_byte_cnt & 0xFF;
    bytes_used ++;
    patt_base_addr[bytes_used] = (patt_byte_cnt >> 8) & 0x7;
    bytes_used ++;

    memset (patt_base_addr + bytes_used, 0, 54);
    bytes_used += 54;

    // Pattern ID;
    for (int i = 0; i < 4; i++) {
        patt_base_addr[bytes_used] = (pattern_id >> (i * 8)) & 0xFF;
        bytes_used ++;
    }

    memcpy (patt_base_addr + bytes_used, config_bytes, config_len);
    bytes_used += config_len;

    // Padding to 64 bytes alignment
    bytes_used --;

    do {
        if ((((uint64_t) (patt_base_addr + bytes_used)) & 0x3F) == 0x3F) { //the last address of the packet stream is 512bit/64byte aligned
            break;
        } else {
            bytes_used ++;
            patt_base_addr[bytes_used] = 0x00; //padding 8'h00 until the 512bit/64byte alignment
        }

    } while (1);

    bytes_used ++;

    return patt_base_addr + bytes_used;
}


void* capi_regex_compile_internal (const char* patt, size_t* size)
{
    // The max size that should be alloc
    // Assume we have at most 1024 lines in a pattern file
    size_t max_alloc_size = MAX_NUM_PATT * (64 +
                                            (PATTERN_WIDTH_BYTES - 4) +
                                            ((PATTERN_WIDTH_BYTES - 4) % 64) == 0 ? 0 :
                                            (64 - ((PATTERN_WIDTH_BYTES - 4) % 64)));

    void* patt_src_base = alloc_mem (64, max_alloc_size);
    //void* patt_src_base = palloc0 (max_alloc_size);
    void* patt_src = patt_src_base;

    elog (DEBUG1, "PATTERN Source Address Start at 0X%016lX\n", (uint64_t) patt_src);

    if (patt == NULL) {
        elog (ERROR, "PATTERN pointer is NULL!\n");
        return NULL;
    }

    //remove_newline (patt);
    // TODO: fill the same pattern for 8 times, workaround for 32x8.
    // TODO: for 64X1, only 1 pattern is needed.
    for (int i = 0; i < 1; i++) {
        elog (DEBUG3, "%s\n", patt);
        patt_src = fill_one_pattern (patt, patt_src, i);
        elog (DEBUG3, "Pattern Source Address 0X%016lX\n", (uint64_t) patt_src);
    }

    elog (DEBUG1, "Total size of pattern buffer used: %ld\n", (uint64_t) ((uint64_t) patt_src - (uint64_t) patt_src_base));

    elog (DEBUG1, "---------- Pattern Buffer: %p\n", patt_src_base);

    if (verbose_level > 2) {
        __hexdump (stdout, patt_src_base, ((uint64_t) patt_src - (uint64_t) patt_src_base));
    }

    (*size) = (uint64_t) patt_src - (uint64_t) patt_src_base;

    return patt_src_base;
}

// The new function based on PostgreSQL storage backend
char* get_attr (HeapTupleHeader tuphdr,
                TupleDesc tupdesc,
                uint16_t lp_len,
                int attr_id,
                int* out_len)
{
    int         nattrs;
    int         off = 0;
    int         i;
    uint16_t    t_infomask  = tuphdr->t_infomask;
    uint16_t    t_infomask2 = tuphdr->t_infomask2;
    int         tupdata_len = lp_len - tuphdr->t_hoff;
    char*       tupdata = (char*) tuphdr + tuphdr->t_hoff;
    bits8*      t_bits = tuphdr->t_bits;

    nattrs = tupdesc->natts;

    if (nattrs < (t_infomask2 & HEAP_NATTS_MASK))
        ereport (ERROR,
                 (errcode (ERRCODE_DATA_CORRUPTED),
                  errmsg ("number of attributes in tuple header is greater than number of attributes in tuple descriptor")));

    if (attr_id >= nattrs) {
        ereport (ERROR,
                 (errcode (ERRCODE_DATA_CORRUPTED),
                  errmsg ("Given index [%d] is out of range, number of attrs: %d", attr_id, nattrs)));
    }

    for (i = 0; i < nattrs; i++) {
        Form_pg_attribute attr;
        bool        is_null;

        attr = TupleDescAttr (tupdesc, i);

        if (i >= (t_infomask2 & HEAP_NATTS_MASK)) {
            is_null = true;
        } else {
            is_null = (t_infomask & HEAP_HASNULL) && att_isnull (i, t_bits);
        }

        if (!is_null) {
            int         len;

            if (attr->attlen == -1) {
                off = att_align_pointer (off, attr->attalign, -1,
                                         tupdata + off);

                if (VARATT_IS_EXTERNAL (tupdata + off) &&
                    !VARATT_IS_EXTERNAL_ONDISK (tupdata + off) &&
                    !VARATT_IS_EXTERNAL_INDIRECT (tupdata + off))
                    ereport (ERROR,
                             (errcode (ERRCODE_DATA_CORRUPTED),
                              errmsg ("first byte of varlena attribute is incorrect for attribute %d", i)));

                len = VARSIZE_ANY (tupdata + off);
            } else {
                off = att_align_nominal (off, attr->attalign);
                len = attr->attlen;
            }

            if (tupdata_len < off + len)
                ereport (ERROR,
                         (errcode (ERRCODE_DATA_CORRUPTED),
                          errmsg ("unexpected end of tuple data")));

            if (i == attr_id) {
                (*out_len) = len;
                break;
            }

            off = att_addlength_pointer (off, attr->attlen,
                                         tupdata + off);
        }
    }

    return (char*) (tupdata + off);
}

int capi_regex_context_init (CAPIContext* context)
{
    if (context == NULL) {
        return -1;
    }

    // Init the job descriptor
    context->card_no      = 0;
    context->timeout      = ACTION_WAIT_TIME;
    context->attach_flags = (snap_action_flag_t) 0;
    context->act          = NULL;

    // Prepare the card and action
    elog (DEBUG1, "Open Card: %d", context->card_no);
    sprintf (context->device, "/dev/cxl/afu%d.0s", context->card_no);
    context->dn = snap_card_alloc_dev (context->device, SNAP_VENDOR_ID_IBM, SNAP_DEVICE_ID_SNAP);

    if (NULL == context->dn) {
        ereport (ERROR,
                 (errcode (ERRCODE_INVALID_PARAMETER_VALUE),
                  errmsg ("Cannot allocate CARD!")));
        return -1;
    }

    elog (DEBUG1, "Start to get action.");
    context->act = get_action (context->dn, context->attach_flags, 5 * context->timeout);
    elog (DEBUG1, "Finish get action.");

    return 0;
}

int capi_regex_job_init (CAPIRegexJobDescriptor* job_desc,
                         CAPIContext* context)
{
    if (NULL == job_desc) {
        return -1;
    }

    if (NULL == context) {
        return -1;
    }

    // Init the job descriptor
    job_desc->context               = context;
    job_desc->patt_src_base         = NULL;
    job_desc->pkt_src_base          = NULL;
    job_desc->stat_dest_base        = NULL;
    job_desc->num_pkt               = 0;
    job_desc->num_matched_pkt       = 0;
    job_desc->pkt_size              = 0;
    job_desc->max_alloc_pkt_size    = 0;
    job_desc->patt_size             = 0;
    job_desc->pkt_size_wo_hw_hdr    = 0;
    job_desc->stat_size             = 0;
    job_desc->pattern               = NULL;
    job_desc->results               = NULL;
    job_desc->curr_result_id        = 0;
    job_desc->start_blk_id          = 0;
    job_desc->num_blks              = 0;
    job_desc->thread_id             = 0;
    job_desc->t_init                = 0;
    job_desc->t_init                = 0;
    job_desc->t_regex_patt          = 0;
    job_desc->t_regex_pkt           = 0;
    job_desc->t_regex_scan          = 0;
    job_desc->t_regex_harvest       = 0;
    job_desc->t_cleanup             = 0;

    job_desc->next_desc             = NULL;

    return 0;
}

int capi_regex_compile (CAPIRegexJobDescriptor* job_desc, const char* pattern)
{
    if (job_desc == NULL) {
        return -1;
    }

    job_desc->patt_src_base = capi_regex_compile_internal (pattern, & (job_desc->patt_size));

    if (job_desc->patt_size == 0 || job_desc->patt_src_base == NULL) {
        return -1;
    }

    return 0;
}

int capi_regex_scan (CAPIRegexJobDescriptor* job_desc)
{
    if (job_desc == NULL) {
        return -1;
    }

    if (capi_regex_scan_internal (job_desc->context->dn,
                                  job_desc->context->timeout,
                                  job_desc->patt_src_base,
                                  job_desc->pkt_src_base,
                                  job_desc->stat_dest_base,
                                  & (job_desc->num_matched_pkt),
                                  job_desc->patt_size,
                                  job_desc->pkt_size,
                                  job_desc->stat_size,
                                  job_desc->thread_id)) {

        ereport (ERROR,
                 (errcode (ERRCODE_INVALID_PARAMETER_VALUE),
                  errmsg ("Hardware ERROR!")));
        return -1;
    }

    return 0;
}

int capi_regex_result_harvest (CAPIRegexJobDescriptor* job_desc)
{
    if (job_desc == NULL) {
        return -1;
    }

    int count = 0;

    // Wait for transaction to be done.
    do {
        action_read (job_desc->context->dn, ACTION_STATUS_L, job_desc->thread_id);
        count++;
    } while (count < 10);

    uint32_t reg_data = action_read (job_desc->context->dn, ACTION_STATUS_H, job_desc->thread_id);
    job_desc->num_matched_pkt = reg_data;
    job_desc->results = (uint32_t*) palloc (reg_data * sizeof (uint32_t));

    //elog (INFO, "Thread %d finished with %d matched packets", job_desc->thread_id, reg_data);

    if (get_results (job_desc->results, reg_data, job_desc->stat_dest_base)) {
        errno = ENODEV;
        return -1;
    }

    return 0;
}

int capi_regex_job_cleanup (CAPIRegexJobDescriptor* job_desc)
{
    if (job_desc == NULL) {
        return -1;
    }

    // TODO: card will be freed in hardware manager
    //snap_detach_action (job_desc->context->act);
    //// Unmap AFU MMIO registers, if previously mapped
    //snap_card_free (job_desc->context->dn);
    //elog (DEBUG2, "Free Card Handle: %p\n", job_desc->context->dn);

    // TODO: patt buffer will be freed in worker
    //free_mem (job_desc->patt_src_base);
    // TODO: packet buffer will be freed in job
    //free_mem (job_desc->pkt_src_base);
    // TODO: dest buffer will be freed in job
    //free_mem (job_desc->stat_dest_base);

    if (job_desc->results) {
        pfree (job_desc->results);
    }

    return 0;
}

bool capi_regex_check_relation (Relation rel)
{
    bool retVal = true;

    /* Check that this relation has storage */
    if (rel->rd_rel->relkind == RELKIND_VIEW) {
        ereport (ERROR,
                 (errcode (ERRCODE_WRONG_OBJECT_TYPE),
                  errmsg ("cannot get raw page from view \"%s\"",
                          RelationGetRelationName (rel))));
        retVal = false;
    }

    if (rel->rd_rel->relkind == RELKIND_COMPOSITE_TYPE) {
        ereport (ERROR,
                 (errcode (ERRCODE_WRONG_OBJECT_TYPE),
                  errmsg ("cannot get raw page from composite type \"%s\"",
                          RelationGetRelationName (rel))));
        retVal = false;
    }

    if (rel->rd_rel->relkind == RELKIND_FOREIGN_TABLE) {
        ereport (ERROR,
                 (errcode (ERRCODE_WRONG_OBJECT_TYPE),
                  errmsg ("cannot get raw page from foreign table \"%s\"",
                          RelationGetRelationName (rel))));
        retVal = false;
    }

    return retVal;
}

void print_result (CAPIRegexJobDescriptor* job_desc, char* header_str, char* out_str)
{
    sprintf (header_str, "num_pkt,pkt_size,init,patt,pkt_cpy,pkt_other,hw_re_scan,harvest,cleanup,hw_perf(MB/s),num_matched_pkt\n");
    sprintf (out_str, "%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%f,%ld",
             job_desc->num_pkt,
             job_desc->pkt_size_wo_hw_hdr,
             job_desc->t_init,
             job_desc->t_regex_patt,
             job_desc->t_regex_pkt_copy,
             job_desc->t_regex_pkt - job_desc->t_regex_pkt_copy,
             job_desc->t_regex_scan,
             job_desc->t_regex_harvest,
             job_desc->t_cleanup,
             perf_calc (job_desc->t_regex_scan / 1000, job_desc->pkt_size_wo_hw_hdr),
             job_desc->num_matched_pkt);
    print_time_text ("|Regex hardware scan|", job_desc->t_regex_scan / 1000, job_desc->pkt_size_wo_hw_hdr);
}

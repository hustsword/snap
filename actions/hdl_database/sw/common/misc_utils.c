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


#include "misc_utils.h"


//static const char* version = GIT_VERSION;
static int verbose_level = 0;


void* alloc_mem (int align, size_t size)
{
    void* a;
    size_t size2 = size + align;

    VERBOSE2 ("%s Enter Align: %d Size: %zu\n", __func__, align, size);

    if (posix_memalign ((void**)&a, 4096, size2) != 0) {
        perror ("FAILED: posix_memalign()");
        return NULL;
    }

    VERBOSE2 ("%s Exit %p\n", __func__, a);
    return a;
}


void free_mem (void* a)
{
    VERBOSE2 ("Free Mem %p\n", a);

    if (a) {
        free (a);
    }
}

int print_results (size_t num_results, void* stat_dest_base)
{
    int i = 0, j = 0;
    uint16_t offset = 0;
    uint32_t pkt_id = 0;
    uint32_t patt_id = 0;
    int rc = 0;

    VERBOSE0 ("---- Result buffer address: %p ----\n", stat_dest_base);
    VERBOSE0 ("---- Results (HW: hardware) ----\n");
    VERBOSE0 ("PKT(HW) PATT(HW) OFFSET(HW)\n");

    for (i = 0; i < (int)num_results; i++) {
        for (j = 0; j < 4; j++) {
            patt_id |= (((uint8_t*)stat_dest_base)[i * 10 + j] << j * 8);
        }

        for (j = 4; j < 8; j++) {
            pkt_id |= (((uint8_t*)stat_dest_base)[i * 10 + j] << (j % 4) * 8);
        }

        for (j = 8; j < 10; j++) {
            offset |= (((uint8_t*)stat_dest_base)[i * 10 + j] << (j % 2) * 8);
        }

        VERBOSE0 ("%7d\t%6d\t%7d\n", pkt_id, patt_id, offset);

        patt_id = 0;
        pkt_id = 0;
        offset = 0;
    }

    return rc;
}

int get_results (void* result, size_t num_matched_pkt, void* stat_dest_base)
{
    int i = 0, j = 0;
    uint32_t pkt_id = 0;

    if (result == NULL) {
        return -1;
    }

    for (i = 0; i < (int)num_matched_pkt; i++) {
        for (j = 4; j < 8; j++) {
            pkt_id |= (((uint8_t*)stat_dest_base)[i * 10 + j] << (j % 4) * 8);
        }

        ((uint32_t*)result)[i] = pkt_id;

        pkt_id = 0;
    }

    return 0;
}

void print_error (const char* file, const char* func, const char* line, int rc)
{
    VERBOSE0 ("ERROR: %s %s failed in line %s with return code %d\n", file, func, line, rc);
}

int64_t diff_time (struct timespec* t_beg, struct timespec* t_end)
{
    if (t_end == NULL || t_beg == NULL) {
        return 0;
    }

    return ((t_end-> tv_sec - t_beg-> tv_sec) * 1000000000L + t_end-> tv_nsec - t_beg-> tv_nsec);
}

float print_time (uint64_t elapsed, uint64_t size)
{
    int t;
    float fsize = (float)size / (1024 * 1024);
    float ft;

    if (elapsed > 10000) {
        t = (int)elapsed / 1000;
        ft = (1000 / (float)t) * fsize;
        VERBOSE0 (" end after %d msec (%0.3f MB/sec)\n", t, ft);
    } else {
        t = (int)elapsed;
        ft = (1000000 / (float)t) * fsize;
        VERBOSE0 (" end after %d usec (%0.3f MB/sec)\n", t, ft);
    }

    return ft;
}

void print_time_text (const char* text, uint64_t elapsed, uint64_t size)
{
    int t;
    float fsize = (float)size / (1024 * 1024);
    float ft;

    if (elapsed > 10000) {
        t = (int)elapsed / 1000;
        ft = (1000 / (float)t) * fsize;

        if (0 == size) {
            VERBOSE0 ("%s run time: %d msec", text, t);
        } else {
            VERBOSE0 ("%s run time: %d msec (%0.3f MB/sec)", text, t, ft);
        }
    } else {
        t = (int)elapsed;
        ft = (1000000 / (float)t) * fsize;

        if (0 == size) {
            VERBOSE0 ("%s run time:  %d usec", text, t);
        } else {
            VERBOSE0 ("%s run time:  %d usec (%0.3f MB/sec)", text, t, ft);
        }
    }
}

float perf_calc (uint64_t elapsed, uint64_t size)
{
    int t;
    float fsize = (float)size / (1024 * 1024);
    float ft;

    t = (int)elapsed / 1000;

    if (t == 0) {
        return 0.0;
    }

    ft = (1000 / (float)t) * fsize;
    return ft;
}

uint64_t get_usec (void)
{
    struct timeval t;

    gettimeofday (&t, NULL);
    return t.tv_sec * 1000000 + t.tv_usec;
}
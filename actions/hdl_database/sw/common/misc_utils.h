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

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <malloc.h>
#include <unistd.h>
#include <time.h>
#include <getopt.h>
#include <ctype.h>

#include "fregex.h"

#define VERBOSE0(fmt, ...) do {         \
        printf(fmt, ## __VA_ARGS__);    \
    } while (0)

#define VERBOSE1(fmt, ...) do {         \
        if (verbose_level > 0)          \
            printf(fmt, ## __VA_ARGS__);    \
    } while (0)

#define VERBOSE2(fmt, ...) do {         \
        if (verbose_level > 1)          \
            printf(fmt, ## __VA_ARGS__);    \
    } while (0)


#define VERBOSE3(fmt, ...) do {         \
        if (verbose_level > 2)          \
            printf(fmt, ## __VA_ARGS__);    \
    } while (0)

#define VERBOSE4(fmt, ...) do {         \
        if (verbose_level > 3)          \
            printf(fmt, ## __VA_ARGS__);    \
    } while (0)


#define PERF_MEASURE(_func, out) \
    do { \
        struct timespec t_beg, t_end; \
        clock_gettime(CLOCK_REALTIME, &t_beg); \
        ERROR_CHECK((_func)); \
        clock_gettime(CLOCK_REALTIME, &t_end); \
        (out) = diff_time (&t_beg, &t_end); \
    } \
    while (0)

#define ERROR_LOG(_file, _func, _line, _rc) \
    do { \
        print_error((_file), (_func), (const char*) (_line), (_rc)); \
    } \
    while (0)

#define ERROR_CHECK(_err) \
    do { \
        int rc = (_err); \
        if (rc != 0) \
        { \
            ERROR_LOG (__FILE__, __FUNCTION__, __LINE__, rc); \
            goto fail; \
        } \
    } while (0)

/*  defaults */
#define STEP_DELAY      200
#define DEFAULT_MEMCPY_BLOCK    4096
#define DEFAULT_MEMCPY_ITER 1
#define ACTION_WAIT_TIME    10   /* Default in sec */
//#define MAX_NUM_PKT 502400
//#define MAX_NUM_PKT 4096
#define MIN_NUM_PKT 4096
#define MAX_NUM_PATT 1024

#define MEGAB       (1024*1024ull)
#define GIGAB       (1024 * MEGAB)

void* alloc_mem (int align, size_t size);
void free_mem (void* a);
int print_results (size_t num_results, void* stat_dest_base);
int get_results (void* result, size_t num_matched_pkt, void* stat_dest_base);
void print_error (const char* file, const char* func, const char* line, int rc);
int64_t diff_time (struct timespec* t_beg, struct timespec* t_end);
float print_time (uint64_t elapsed, uint64_t size);
void print_time_text (const char* text, uint64_t elapsed, uint64_t size);
float perf_calc (uint64_t elapsed, uint64_t size);
uint64_t get_usec (void);



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

#include "boost/make_shared.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/chrono.hpp"
#include "Interface.h"
#include "HardwareManager.h"
#include "WorkerRegex.h"
#include "BufRegex.h"
#include "JobRegex.h"

using namespace boost::chrono;

int start_regex_workers (test_params in_params)
{
    std::cout << "Running with job manager" << std::endl;

    HardwareManagerPtr hw_mgr =  boost::make_shared<HardwareManager> (in_params.card_no, 0, 1000);
    WorkerRegexPtr worker = boost::make_shared<WorkerRegex> (hw_mgr, in_params.debug);
    worker->set_mode (INTERRUPT == in_params.mode);
    worker->set_job_start_threshold (in_params.buf_num * in_params.job_num);

    return 0;
}

void print_test_params (test_params in_params)
{
    const char* mode_str[] = {"POLL", "INTERRUPT", "INVALID"};
    printf ("card_no:\t%d\n", in_params.card_no);
    printf ("job_num:\t%d\n", in_params.job_num);
    printf ("buf_num:\t%d\n", in_params.buf_num);
    printf ("memcopy_size:\t%d\n", in_params.memcopy_size);
    printf ("timeout:\t%d\n", in_params.timeout);
    printf ("mode:\t%s\n", mode_str[in_params.mode]);
    printf ("debug:\t%d\n", in_params.debug);
}


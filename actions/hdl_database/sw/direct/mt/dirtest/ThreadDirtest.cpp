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

#include <malloc.h>
#include "ThreadDirtest.h"
#include "JobDirtest.h"

ThreadDirtest::ThreadDirtest()
    : ThreadBase (0, 600)
{
    printf("create dirtest thread\n");
}

ThreadDirtest::ThreadDirtest (int in_id)
    : ThreadBase (in_id)
{
    printf("create dirtest thread\n");
}

ThreadDirtest::ThreadDirtest (int in_id, int in_timeout)
    : ThreadBase (in_id, in_timeout)
{
    printf("create dirtest thread\n");
}

ThreadDirtest::~ThreadDirtest()
{
}

int ThreadDirtest::init()
{
    printf("init thread\n");
    for (size_t i = 0; i < m_jobs.size(); i++) {
        if (m_jobs[i]->init()) {
            return -1;
        }
    }

    return 0;
}

void ThreadDirtest::work_with_job (JobPtr in_job)
{
    printf("thread work with job\n");
    JobDirtestPtr job = boost::dynamic_pointer_cast<JobDirtest> (in_job);

    if (NULL == job) {
        printf ("ERROR: Failed to get pointer to JobDirtest\n");
        return;
    }

    do {
        if (0 != job->run()) {
            printf ("ERROR: Failed to run the JobDirtest\n");
            return;
        }
    } while (0);

    return;
}

void ThreadDirtest::cleanup()
{
    printf("clean up thread\n");
    for (size_t i = 0; i < m_jobs.size(); i++) {
        m_jobs[i]->cleanup();
    }
}

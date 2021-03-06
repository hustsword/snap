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
#ifndef THREADREGEX_H_h
#define THREADREGEX_H_h

#include <iostream>
#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include "ThreadBase.h"
#include "HardwareManager.h"

class ThreadRegex : public ThreadBase
{
public:
    // Constructor of thread regex
    ThreadRegex();
    ThreadRegex (int in_id);
    ThreadRegex (int in_id, int in_timeout);

    // Destructor of thread regex
    ~ThreadRegex();

    // Initialize each jobs in this thread
    virtual int init();

    // Work with the jobs
    virtual void work_with_job (JobPtr in_job);

    // Cleanup
    virtual void cleanup();

private:
};

typedef boost::shared_ptr<ThreadRegex> ThreadRegexPtr;

#endif

/*
 * File: thread.cpp
 * Copyright 2017 Terrain Data, Inc.
 *
 * This file is part of FRC, a fast reference counting library for C++
 * (see <https://github.com/terraindata/frc>).
 *
 * FRC is distributed under the MIT License, which is found in
 * COPYING.md in the repository.
 *
 * You should have received a copy of the MIT License
 * along with FRC.  If not, see <https://opensource.org/licenses/MIT>.
 */

#include <thread>
#include "platformSpecific.h"
#include "thread.h"



#if defined(__APPLE__)
#include <sys/sysctl.h>

#elif defined(TERRAIN_WINDOWS)
#include <stdlib.h>
#include <windows.h>
#include <stdexcept>

#elif defined(TERRAIN_LINUX)
#include <stdio.h>
#include <pthread.h>
#include <sched.h>
#include <stdexcept>

#elif defined(TERRAIN_FREEBSD)
#include <sys/param.h>
#include <sys/cpuset.h>
#include <stdexcept>

#else
#error "Unrecognized platform"
#endif

namespace terrain
{
sz hardwareConcurrency()
{
    return std::thread::hardware_concurrency();
}

#if defined(__APPLE__)
size_t cache_line_size()
{
    size_t line_size = 0;
    size_t sizeof_line_size = sizeof(line_size);
    sysctlbyname("hw.cachelinesize", &line_size, &sizeof_line_size, 0, 0);
    return line_size;
}

#elif defined(TERRAIN_WINDOWS)

size_t cache_line_size()
{
    size_t line_size = 0;
    DWORD buffer_size = 0;
    DWORD i = 0;
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION* buffer = 0;

    GetLogicalProcessorInformation(0, &buffer_size);
    buffer = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION*) emalloc(buffer_size);
    GetLogicalProcessorInformation(&buffer[0], &buffer_size);

    for(i = 0; i != buffer_size / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
            ++i)
    {
        if(buffer[i].Relationship == RelationCache
                && buffer[i].Cache.Level == 1)
        {
            line_size = buffer[i].Cache.LineSize;
            break;
        }
    }

    free(buffer);
    return line_size;
}

#elif defined(TERRAIN_LINUX)

size_t cache_line_size()
{
    FILE* p = 0;
    p = fopen("/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size",
              "r");
    unsigned int i = 0;
    if(p)
    {
        if(!fscanf(p, "%d", &i))
            i = 64;
        fclose(p);
    }
    return i;
}

#else
#error "Unrecognized platform"
#endif

//TODO: make bindToProcessor() work for more than 64 cores!

#ifdef TERRAIN_WINDOWSGCCx64
void bindToProcessor(sz n)
{
    if(::SetThreadAffinityMask(::GetCurrentThread(), (DWORD_PTR)1 << n) == 0)
        throw std::runtime_error("::SetThreadAffinityMask() failed");
}

#elif defined TERRAIN_LINUXGCCx64
void bindToProcessor(sz n)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(n, &cpuset);

    int errno_(
        ::pthread_setaffinity_np(::pthread_self(), sizeof(cpuset),
                                 &cpuset));
    if(errno_ != 0)
        throw std::runtime_error("::pthread_setaffinity_np() failed");
}

#elif defined TERRAIN_BSDGCCx64
void bindToProcessor(sz n)
{
    cpuset_t cpuset;
    CPU_ZERO(& cpuset);
    CPU_SET(n, & cpuset);

    if(::cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1, sizeof(cpuset),
                            & cpuset) == -1)
        throw std::runtime_error("::cpuset_setaffinity() failed");
}

#else
#error "Unrecognized platform"
#endif

} /* namespace terrain */

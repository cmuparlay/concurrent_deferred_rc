/*
 * File: FRCManager.cpp
 * Copyright 2016-2018 Terrain Data, Inc.
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

#include <util/thread.h>
#include <util/FastRNG.h>

#include "FRCManager.h"
#include "ThreadData.h"
#include "../frc.h"
#include <util/Exception.h>

namespace terrain
{
namespace frc
{
namespace detail
{

DestructorMap& getDestructorMap()
{
    static DestructorMap s_destructorMap;

    return s_destructorMap;
}

//this is left uninitialized for performance reasons
tls(ThreadData*, threadData);

//this is used to track recursive registrations
thread_local sz threadDataRegistrationCount = 0;

FRCManager& getFRCManager()
{
    static FRCManager s_manager;
    return s_manager;
}

FRCManager& dummy = getFRCManager();

FRCManager::FRCManager() :
    helpRouter(2 * hardwareConcurrency())
{
    getDestructorMap(); // we need to make sure the destructorMap has a lifetime that exceeds the FRCManager
    writeFence();
}

FRCManager::~FRCManager()
{
    auto td = registerThread();
    helpRouter.collect(td);
    unregisterThread();
    delete td;
}

ThreadData* FRCManager::registerThread()
{
    if(isThreadRegistered())
    {
        //already registered
        ++threadDataRegistrationCount;
        return threadData;
    }

    threadData = new ThreadData();
    threadDataRegistrationCount = 1;

    getFRCManager().helpRouter.addThread(threadData);

    if(debug)
        dprint("registerThread: %p\n", threadData);

    return threadData;
}

void FRCManager::unregisterThread()
{
    assert(isThreadRegistered());

    if(threadDataRegistrationCount == 1)
    {
        auto& manager = getFRCManager();
        for(sz i = 0; i < FRCConstants::numTryHelpCallsOnUnregister; ++i)
            manager.help();

        threadData->detach();
        threadData = nullptr;
    }

    --threadDataRegistrationCount;
}

bool FRCManager::isThreadRegistered() noexcept
{
    return threadDataRegistrationCount > 0;
}

void FRCManager::help()
{
    if(!isThreadRegistered())
        return;
    threadData->help();
}


void FRCManager::collect()
{
    FRCToken token;
    getFRCManager().helpRouter.collect(threadData);
}

bool isThreadRegistered() noexcept
{
    return FRCManager::isThreadRegistered();
}

} /* namespace detail */
} /* namespace frc */
} /* namespace terrain */

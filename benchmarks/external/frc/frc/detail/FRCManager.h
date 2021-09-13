/*
 * File: FRCManager.h
 * Copyright 2014-2018 Terrain Data, Inc.
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

#pragma once

#include <cassert>

#include "../../util/util.h"
#include "../../synchronization/MutexSpin.h"

#include "FRCConstants.h"
#include "ThreadData.h"
#include "DestructorMap.h"
#include "ObjectHeader.h"
#include "HelpRouter.h"


namespace terrain
{
namespace frc
{
namespace detail
{

/**
 * this shell class ensures that the map is statically
 * initialized before destructors are registered in it.
 */
extern DestructorMap destructorMap;

class FRCManager;


static constexpr bool debug = false;
static constexpr bool debugExtra = false;


extern tls(ThreadData*, threadData);

FRCManager& getFRCManager();

class FRCManager
{
private:
    static constexpr bool debug = false;
    static constexpr bool debugExtra = false;

public:
    FRCManager();

    FRCManager(FRCManager const&) = delete;

    FRCManager(FRCManager&&) = delete;

    FRCManager& operator=(FRCManager const&) = delete;

    FRCManager& operator=(FRCManager&&) = delete;

    ~FRCManager();

    void help();

public:

    static void collect();

    static ThreadData* registerThread();

    static void unregisterThread();

    static bool isThreadRegistered() noexcept;

private:

    HelpRouter helpRouter;
};

extern bool isThreadRegistered() noexcept;

} /* namespace detail */
} /* namespace frc */
} /* namespace terrain */

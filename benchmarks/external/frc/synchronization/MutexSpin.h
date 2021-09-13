/*
 * File: MutexSpin.h
 * Copyright 2013-2018 Terrain Data, Inc.
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

#include <thread>
#include <mutex>

#include "../util/util.h"

#include "Spinner.h"

namespace terrain
{

/**
 * A exponentially backed off spinlock with low overhead when uncontended.
 * Uses random jitter to reduce rematch problems when contended.
 */
class MutexSpin
{
private:
    static constexpr sz maxSpins = 2048 - 1;

public:

    constexpr MutexSpin() noexcept
        : locked(false)
    {
        //locked.store(false, orls);
    }

    MutexSpin(MutexSpin const&) = delete;

    MutexSpin(MutexSpin&&) = delete;

    void lock() noexcept
    {
        if(!locked.exchange(true, oarl))
            return;

        slowLock();
    }

    bool try_lock() noexcept
    {
        return !locked.load(ocon) && !locked.exchange(true, oarl);
    }

    void unlock() noexcept
    {
        locked.store(false, orls);
    }

    auto acquire()
    {
        return std::unique_lock<MutexSpin>(*this);
    }

    auto try_acquire()
    {
        return std::unique_lock<MutexSpin>(*this, std::try_to_lock);
    }

private:

    void slowLock() noexcept
    {
        if(Spinner::spin(
                    [&]()
    {
        return try_lock();
        },
        1024 * 128))
        return;

        while(!try_lock())
            std::this_thread::yield();
    }

private:
    atm<bool> locked;
};

}

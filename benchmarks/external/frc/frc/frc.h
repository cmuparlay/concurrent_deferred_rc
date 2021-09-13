/*
 * File: frc.h
 * Copyright 2016-7 Terrain Data, Inc.
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

#include "detail/FRCManager.h"

#include "AtomicPointer.h"
#include "SharedPointer.h"
#include "PrivatePointer.h"

namespace terrain
{
namespace frc
{


template<class T>
using ap = AtomicPointer<T>;

template<class T>
using sp = SharedPointer<T>;

template<class T>
using hp = PrivatePointer<T>;

inline static bool isThreadRegistered()
{
    return detail::FRCManager::isThreadRegistered();
}

/**
 * A wrapper class for use when entering and exiting FRC code.
 * Just stack allocate a FRCToken, which will register the thread.
 * When the token goes out of scope, it will be destructed, thus
 * unregistering the thread.
 */
struct FRCToken
{

    FRCToken()
    {
        detail::FRCManager::registerThread();
    }

    FRCToken(FRCToken const&) = delete;
    FRCToken(FRCToken&&) = delete;
    FRCToken& operator=(FRCToken const&) = delete;
    FRCToken& operator=(FRCToken&&) = delete;

    ~FRCToken()
    {
        detail::FRCManager::unregisterThread();
    }
};

template<class T, class ... Args>
auto make_atomic(Args&& ... args)
{
    ap<T> result;
    result.makeProtected(std::forward<Args>(args) ...);
    return result;
}

template<class T, class ... Args>
auto make_shared(Args&& ... args)
{
    sp<T> result;
    result.make(std::forward<Args>(args) ...);
    return result;
}

template<class T, class ... Args>
auto make_protected(Args&& ... args)
{
    hp<T> result;
    result.make(std::forward<Args>(args)...);
    return result;
}

} /* namespace frc */

} /* namespace terrain */

/*
 * File: PinSet.h
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

#include "../../util/util.h"

#include <assert.h>

#include "../../util/tls.h"

#include "FRCConstants.h"

namespace terrain
{
namespace frc
{
namespace detail
{

extern tls(atm<void*>*, head);

extern bool isThreadRegistered() noexcept;

class PinSet
{
private:
    static sz constexpr size = FRCConstants::pinSetSize;

public:

    PinSet();

    /**
     * acquires a pin
     */
    static atm<void*>* acquire() noexcept
    {
        assert(isThreadRegistered());
        assert(head != nullptr); // all pins acquired

        auto value = head;
        auto next = (atm<void*>*) value->load(orlx);
        head = next;
        return value;
    }

    /**
     * releases a pin
     */
    static void release(atm<void*>* value) noexcept
    {
        assert(isThreadRegistered());

        auto next = head;
        value->store(next, orlx);
        head = value;
    }

    static void setProtectedPointer(atm<void*>* value) noexcept
    {
        head = value;
    }

    static auto getProtectedPointer() noexcept
    {
        return head;
    }

    bool isValid(void* ptr) noexcept
    {
        return ptr != nullptr &&
               ((sz) ptr < (sz) &protectedObjects[0] ||
                (sz) ptr >= (sz) &protectedObjects[size]);
    }

public:
    std::unique_ptr<atm<void*>[]> protectedObjects;
};

}
}
}

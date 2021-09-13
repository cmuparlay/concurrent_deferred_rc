/*
 * File: StaticTreeRouter.cpp
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

#include <util/bitTricks.h>
#include <util/FastRNG.h>
#include <util/DebugPrintf.h>
#include <iostream>

#include "StaticTreeRouter.h"

namespace terrain
{

StaticTreeRouter::StaticTreeRouter(uint numInputs)
{
    this->numInputs = numInputs;
    numNodes = roundUpToPowerOfTwo(std::max(uint(2), numInputs));
    nodes.reset(new INode[numNodes]);
    writeFence();
}

bool StaticTreeRouter::valid() const noexcept
{
    readFence();
    uint code = 3 & nodes[rootIndex]->status;
    bool root_empty = code == 0;

    for(uint i = 0; i < numInputs; i += 2)
    {
        auto index = getParentIndex(getInputIndex(i));
        for(;;)
        {
            if((3 & (nodes[index]->status)) > 0)
                if(root_empty && index != rootIndex)
                    return false;

            if(((index & 1) != 0) || index <= rootIndex)
                break;
            index = getParentIndex(index);
        }
    }
    return true;
}

uint StaticTreeRouter::getParentIndex(uint index) noexcept
{
    return index / 2;
}

uint StaticTreeRouter::getLeftChildIndex(uint index) noexcept
{
    return index * 2;
}

uint StaticTreeRouter::getRightChildIndex(uint index) noexcept
{
    return getLeftChildIndex(index) + 1;
}

uint StaticTreeRouter::getInputIndex(uint index) const noexcept
{
    return index + numNodes;
}

/**
 * even numbers are left children
 */
uint StaticTreeRouter::getMask(uint index) noexcept
{
    return (index & 1) + 1;
}

uint StaticTreeRouter::getSiblingIndex(uint index) noexcept
{
    return index ^ 1;
}

bool StaticTreeRouter::cyclicRelease(uint index) noexcept
{
    return doCyclicRelease(getInputIndex(index));
}

bool StaticTreeRouter::doCyclicRelease(uint childIndex) noexcept
{
    auto parentIndex = getParentIndex(childIndex);
    auto mask = getMask(childIndex);
    Node& node = nodes[parentIndex].get();

    auto guard = node.lock.acquire();
    node.status ^= mask; // toggle mask bit

    if(node.status == 1 || node.status == 2)
        return false; //stop propagating signal

    if(parentIndex > rootIndex)
        doRelease(parentIndex);
    return true; //root was cycled
}

bool StaticTreeRouter::release(uint index) noexcept
{
    return doRelease(getInputIndex(index));
}

bool StaticTreeRouter::doRelease(uint childIndex) noexcept
{
    auto parentIndex = getParentIndex(childIndex);
    auto mask = getMask(childIndex);
    Node& node = nodes[parentIndex].get();

    auto guard = node.lock.acquire();

    if((node.status & mask) == 0)
        return false; //already cleared

    node.status &= ~mask;

    if(node.status != 0)
        return false; //stop propagating signal

    if(parentIndex <= rootIndex)
        return true; //root status became zero

    return doRelease(parentIndex);
}

bool StaticTreeRouter::acquire(uint index) noexcept
{
    return doAcquire(getInputIndex(index));
}

bool StaticTreeRouter::doAcquire(uint childIndex) noexcept
{
    auto parentIndex = getParentIndex(childIndex);
    auto mask = getMask(childIndex);
    Node& node = nodes[parentIndex].get();

    auto guard = node.lock.acquire();

    if((node.status & mask) != 0)
        return false; //already acquired

    auto prev = node.status;
    node.status |= mask;

    if(parentIndex <= rootIndex)
        return (prev == 0); //root status became non-zero

    return doAcquire(parentIndex);
}

uint StaticTreeRouter::findAcquired(std::memory_order mo) const noexcept
{
    do
    {
        auto result = tryFindAcquired(mo);
        if(result != notFound)
            return result;
    }
    while(status(oacq));

    return notFound;
}

uint StaticTreeRouter::tryFindAcquired(std::memory_order mo) const noexcept
{
    std::atomic_thread_fence(mo);
    uint salt = FastRNG::next();
    uint index = rootIndex;
    for(;;)
    {
        if(index >= numNodes)
        {
            return index - numNodes;
        }

        uint code = 3 & nodes[index]->status;
        index = getLeftChildIndex(index);
        switch(code)
        {
            case(0): //empty
                return notFound;

            case(1): //left
                break;

            case(2): //right
                ++index;
                break;

            case(3): //both
            default:
                index += (salt & 1);
                salt /= 2;
                break;
        }
    }
}

void StaticTreeRouter::print() const
{
    for(uint i = 0; i < numInputs; i += 2)
    {
        auto index = getParentIndex(getInputIndex(i));
        for(;;)
        {
            dprint("%llu ", nodes[index]->status);

            if(((index & 1) != 0) || index <= rootIndex)
                break;
            index = getParentIndex(index);
        }

        dprint("\n");
    }
}


} /* namespace terrain */

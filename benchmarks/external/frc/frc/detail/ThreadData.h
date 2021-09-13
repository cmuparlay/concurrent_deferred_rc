/*
 * File: ThreadData.h
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

#include <assert.h>

#include <vector>

#include "../../util/tls.h"
#include "../../synchronization/MutexSpin.h"

#include "ObjectHeader.h"
#include "PinSet.h"

namespace terrain
{
namespace frc
{
namespace detail
{

class ThreadData;

//this is left uninitialized for performance reasons
extern tls(ThreadData*, threadData);

struct ScanFlag
{
    std::atomic<bool> flag;

    constexpr ScanFlag() : flag(false)
    {}
};

extern tls(ScanFlag, scanFlag);

//this is used to track recursive registrations
extern thread_local sz threadDataRegistrationCount;

extern bool isThreadRegistered() noexcept;

inline static void registerDecrement(void* ptr) noexcept;

inline static void registerIncrement(void* ptr) noexcept;

class HelpRouter;

/**
 * help flow:
 *  - inc/dec triggers help() call
 *  - try to process worker thread data
 *  - on failure, try to process another thread data
 *  	- first try own work queue
 *  	- use a static tree router to find a nonempty work queue
 *  	- move thread data to own work queue on success
 */
class ThreadData
{
private:
    static constexpr bool debug = false;
    static constexpr bool debugExtra = false;

public:
    //MutexSpin pinMutex;

public:

    explicit ThreadData();

    ThreadData(ThreadData const&) = delete;

    ThreadData(ThreadData&&) = delete;

    ThreadData& operator=(ThreadData const&) = delete;

    ThreadData& operator=(ThreadData&&) = delete;

    ~ThreadData();

    void registerDecrement(void* ptr) noexcept
    {
        if(!ptr)
            return;

        auto header = getObjectHeader(ptr);
        if(debugExtra)
            dout("ThreadData::registerDecrement() ", this, " ", threadData, " ", ptr, " ",
                 header, " ", header->getCount());

        if(!header->tryDecrement())
        {
            logDecrement(header);
        }
    }

    void logDecrement(ObjectHeader* header) noexcept
    {
        decrementBuffer[decrementIndex] = header;
        ++decrementIndex;
        if(decrementIndex == helpIndex)
            help();
    }

    template<class PostDequeueHandler>
    bool tryHelp(byte phase, PostDequeueHandler&& postDequeueHandler)
    {
        if(phase == FRCConstants::scan)
            return scan(std::forward<PostDequeueHandler>(postDequeueHandler));
        return sweep(std::forward<PostDequeueHandler>(postDequeueHandler));
    }

    void help();

    void detach()
    {
        lastHelpIndex = decrementIndex;
        detached.store(true, orls);
        writeFence();
    }

    bool isReadyToDestruct()
    {
        return allWorkComplete() && detached.load(oacq);
    }

    bool allWorkComplete()
    {
        return decrementStack.size() == 0 &&
               decrementStackIndex == 0 &&
               decrementIndex == decrementCaptureIndex;
    }

    static void waitForScan()
    {
        while(scanFlag.flag.load(oacq))
        {
            dout("waitForScan");
            Relaxer::relax();
        }
    }

    static bool isScanning()
    {
        return scanFlag.flag.load(oacq);
    }

    void protect(void* ptr)
    {
        if(!pinSet.isValid(ptr))
            return;

        auto header = getObjectHeader(ptr);
        header->increment();
        detail::threadData->logDecrement(header); //won't be processed until next epoch
        if(debugExtra) dout("ThreadData::protect() ", this, " ", header);
    }

private:
    static constexpr auto numScanBlocks =
        (uint)(FRCConstants::pinSetSize / FRCConstants::protectedBlockSize);

    template<class PostDequeueHandler>
    bool scan(PostDequeueHandler&& postDequeueHandler)
    {
        if(debug) dout("ThreadData::scan() ", this);
        readFence();

        auto begin = lastScanIndex;
        auto end = begin + FRCConstants::protectedBlockSize;
        lastScanIndex = end;
        postDequeueHandler(lastScanIndex >= FRCConstants::pinSetSize);

        if(debug) dout("ThreadData::scan() success ", this, " ", begin, "-", end);
        auto protectedPtrs = this->pinSet.protectedObjects.get();

        {
            scanFlag.flag.store(true, orls);
            for(auto i = begin; i < end; ++i)
            {
                void* ptr;

                do
                    ptr = protectedPtrs[i].load(oacq);
                while(ptr == (void*) FRCConstants::busySignal);

                protect(ptr);
            }
            scanFlag.flag.store(false, orls);
        }
        if(debug) dout("ThreadData::scan() done ", this, " ", begin, "-", end);
        //writeFence();

        //TODO: could possibly eliminate the last write here
        if(numRemainingScanBlocks.fetch_sub(1, oarl) > 1)
            return false;

        if(debug) dout("ThreadData::scan() completed ", this, " ", begin, "-", end);
        return true;
    }

    template<class PostDequeueHandler>
    bool sweep(PostDequeueHandler&& postDequeueHandler)
    {
        readFence();
        if(debug) dout("ThreadData::sweep() ", this);
        auto blockSize = std::min(FRCConstants::logBlockSize,
                                  decrementStackIndex - decrementStackTarget);
        auto begin = decrementStackIndex;
        decrementStackIndex -= blockSize;
        postDequeueHandler(decrementStackIndex <= decrementStackTarget);

        //success: dequeued a block
        if(debug) dout("ThreadData::sweep() success ", this, " ", begin, "-", blockSize);

        for(sz i = 0; i < blockSize; ++i)
        {
            auto h = decrementStack[begin - 1 - i];
            if(debugExtra) dout("ThreadData::sweep() decrement ", this, " ", h);
            h->decrementAndDestroy();
        }

        //TODO: could possibly eliminate the last write here
        if(numRemainingDecrementBlocks.fetch_sub(1, oarl) > 1)
            return false;

        decrementStack.resize(decrementStackIndex);

        //reset decrement capture index -- this captures the next group of decrements


        auto from = decrementCaptureIndex;
        auto to = lastHelpIndex.load(oacq);
        for(sz i = from; i != to; i = (i + 1) & FRCConstants::logMask)
        {
            decrementStack.push_back(decrementBuffer[i]);
        }

        decrementCaptureIndex = to;

        decrementStackIndex = decrementStack.size();
        decrementStackTarget = decrementStackIndex - std::min(FRCConstants::logBlockSize * 4,
                               decrementStackIndex);
        auto numSweepBlocks = (intt) ceilPositiveNoOverflow(
                                  decrementStackIndex - decrementStackTarget,
                                  FRCConstants::logBlockSize);
        numRemainingDecrementBlocks.store(numSweepBlocks, orlx);

        //reset scan vars
        lastScanIndex = 0;
        numRemainingScanBlocks.store(numScanBlocks, orls);

        if(debug)
            dout("ThreadData::sweep() completed ", this, " ", decrementStackIndex, " ",
                 decrementStackTarget,
                 " ", numSweepBlocks);

        //    if (decrementStackIndex > 20000 )//|| numSweepBlocks > 1)
        //      dout(this, " ", decrementStackIndex, " ",
        //           decrementStackTarget,
        //           " ", numSweepBlocks);

        writeFence();
        return true;
    }

    sz bufferSeparation(sz from, sz to)
    {
        return (from <= to) ?
               (to - from) :
               (to + (FRCConstants::logBufferSize - from));
    }

private:

    sz decrementIndex;
    sz helpIndex;
    std::unique_ptr<ObjectHeader *[]> decrementBuffer;
    std::vector<ObjectHeader*> decrementStack;
    PinSet pinSet;
    bool helping;
    cacheLinePadding padding0;

    atm<sz> lastHelpIndex;
    sz lastScanIndex; //queues scan tasks
    sz decrementConsumerIndex; //dequeues sweep tasks
    sz decrementCaptureIndex; //queues sweep tasks
    sz decrementStackIndex;
    sz decrementStackTarget;

public:
    uint lastPhaseDispatched;
    uint subqueue;
private:
    cacheLinePadding padding2;

    atm<intt> numRemainingDecrementBlocks;
    atm<uint> numRemainingScanBlocks;
    cacheLinePadding padding3;

    atm<bool> detached;

public:
    HelpRouter* helpRouter;
    cacheLinePadding padding4;
};

inline static void registerDecrement(void* ptr) noexcept
{
    assert(isThreadRegistered());
    threadData->registerDecrement(ptr);
}

inline static void registerIncrement(void* ptr) noexcept
{
    if(ptr)
        getObjectHeader(ptr)->increment();
}

} /* namespace detail */
} /* namespace frc */
} /* namespace terrain */

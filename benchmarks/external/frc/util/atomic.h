/*
 * File: atmomic.h
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

#pragma once

#include <atomic>
#include <utility>
#include "types.h"

/**
 * Shortcuts and aliases for writing concurrent, atomic memory accesses.
 * Fences and cache line sizing.
 */
namespace terrain
{

/**
 * Cache line size (true for modern Intel processors)
 */
constexpr sz cacheLineSize = 64; //64 byte cache line (Core and newer)

/**
 * Memory order aliases
 */

const std::memory_order orlx = std::memory_order_relaxed;
const std::memory_order ocon = std::memory_order_consume;
const std::memory_order oacq = std::memory_order_acquire;
const std::memory_order orls = std::memory_order_release;
const std::memory_order oarl = std::memory_order_acq_rel;
const std::memory_order oseq = std::memory_order_seq_cst;

/**
 * A write fence (release memory order)
 */
inline void writeFence()
{
    std::atomic_thread_fence(orls);
}

/**
 * A consume fence (consume memory order)
 */
inline void consumeFence()
{
    std::atomic_thread_fence(ocon);
}

/**
 * A read fence (acquire memory order)
 */
inline void readFence()
{
    std::atomic_thread_fence(oacq);
}

/**
 * A read-write fence (acquire-release memory order)
 */
inline void rwfence()
{
    std::atomic_thread_fence(oarl);
}

/**
 * A full memory fence (sequential memory order)
 */
inline void fence()
{
    std::atomic_thread_fence(oseq);
}

/**
 * A class which is the size of a cache line.
 */
struct cacheLinePadding
{
    char padding[cacheLineSize - 1];
};

/**
 * Template alias for std::atomic
 */
template<typename T>
using atm = std::atomic<T>;

typedef std::memory_order memoryOrder;

}

#include "Isolate.h"

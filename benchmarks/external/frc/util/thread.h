/*
 * File: thread.h
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

#include <thread>
#include "types.h"

/**
 * This is where low-level thread control and information functions are defined
 */

namespace terrain
{

/**
 * @return the number of hardware threads on the current machine
 */
extern sz hardwareConcurrency();

/**
 * @return the cache line size (in bytes) of the processor, or 0 on failure
 */
size_t cache_line_size();

/**
 * Bind the current thread to the given hardware thread ("processor").
 * Binding a thread to a particular processor ensures that it wont be swapped
 * off of that hardware unit during execution. By ensuring this, typically
 * task switching costs and rates are reduced as well as cache misses.
 *
 * If running a thread pool, the best performance is typically achieved by
 * evenly distributing and binding each thread across the hardware processors.
 */
extern void bindToProcessor(sz processorID);

}

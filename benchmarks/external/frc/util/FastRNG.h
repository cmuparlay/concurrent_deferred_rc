/*
 * File: FastRNG.h
 * Copyright 2017-2018 Terrain Data, Inc.
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

#include <chrono>
#include "types.h"
#include "tls.h"

#include "DebugPrintf.h"

namespace terrain
{

//http://software.intel.com/en-us/articles/fast-random-number-generator-on-the-intel-pentiumr-4-processor
//http://en.wikipedia.org/wiki/Linear_congruential_generator

class FastRNG;

namespace detail
{
extern tls(un, fastRNGSeeds);
}

using namespace detail;

/**
 * A fast, thread-local random number generator.
 * Using a fast LCG generator and therefore does not have ideal properties
 * for an RNG. Uniform distributions and inter-sample independence is traded off
 * here somewhat to achieve high efficiency. Use another RNG if you need
 * high quality samples. If you need performance, you came to the right place.
 */
class FastRNG
{
private:

    inline static un getNext()
    {
        un seed_ = (6364136223846793005ll * fastRNGSeeds
                    + 1442695040888963407ll);
        fastRNGSeeds = seed_;
        return (seed_ >> 16ll) ^ (seed_ << (63ll - 16ll));
    }

    inline static un getNext(un max)
    {
        return getNext() % max;
    }

public:

    static un seed();

    static un seed(un value)
    {
        return fastRNGSeeds = value;
    }

    /**
     * @return next pseudo-uniformly pseudo-randomly generated number integer from [0, std::numeric_limits<un>::max())
     */
    inline static un next()
    {
        return getNext();
    }

    /**
     * @return next pseudo-uniformly pseudo-randomly generated integer from [0, max)
     */
    inline static un next(un max)
    {
        return getNext(max);
    }

    /**
     * @return next pseudo-uniformly pseudo-randomly generated integer from [min, max)
     */
    inline static un next(un min, un max)
    {
        return min + getNext(max - min);
    }

    /**
     * @return next pseudo-uniformly pseudo-randomly generated number integer from [0, std::numeric_limits<uint>::max())
     */
    inline static uint nextUInt()
    {
        return (uint)next();
    }

    /**
     * @return next pseudo-uniformly pseudo-randomly generated integer from [0, max)
     */
    inline static uint nextUInt(uint max)
    {
        return (uint)next(max);
    }

    /**
     * @return next pseudo-uniformly pseudo-randomly generated integer from [min, max)
     */
    inline static uint nextUInt(uint min, uint max)
    {
        return (uint)getNext(max - min);
    }
};

}

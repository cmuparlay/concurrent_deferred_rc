/*
 * File: Spinner.h
 * Copyright 2013, 2017 Terrain Data, Inc.
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

#include "../util/FastRNG.h"
#include "../util/util.h"

#include "Relaxer.h"

namespace terrain
{

struct Spinner
{

    template<class TryFunction>
    static bool spin(TryFunction tryFunc, sz const maxSpins)
    {
        //exponential back-off with jitter
        sz totalSpins = 0;
        for(sz base = 32;;)
        {
            sz numSpins = FastRNG::next(base);
            for(volatile sz j = numSpins; j > 0; --j)
                Relaxer::relax();

            if(tryFunc())
                return true;

            totalSpins += numSpins;

            base *= 4;
            if(totalSpins + base / 2 >= maxSpins)
                break;
        }

        return false;
    }
};

}

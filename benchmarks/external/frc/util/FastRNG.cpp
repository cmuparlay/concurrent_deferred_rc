/*
 * File: FastRNG.cpp
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

#include "FastRNG.h"
#include "getticks.h"

namespace terrain
{

namespace detail
{
tls(un, fastRNGSeeds);
}

un FastRNG::seed()
{
    return seed((un)getticks());
}

} // namespace terrain

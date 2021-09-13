/*
 * Copyright 2014, 2017 Terrain Data, Inc.
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

#include "PinSet.h"

namespace terrain
{
namespace frc
{
namespace detail
{

tls(atm<void*>*, head);

PinSet::PinSet() :
    protectedObjects(new atm<void*>[size])
{
    for(sz i = 0; i < (size - 1); ++i)
        protectedObjects[i].store(&protectedObjects[i + 1], orlx);
    protectedObjects[size - 1].store(nullptr, orls);

    head = &protectedObjects[0];
}


} /* namespace detail */
} /* namespace frc */
} /* namespace terrain */


/*
 * File: Relaxer.h
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

#include "../util/platformSpecific.h"

namespace terrain
{

/**
 *
 */
struct Relaxer
{
public:

#ifdef TERRAIN_X64
    static void relax() noexcept
    {
        //low power pause instruction
        asm volatile("pause;": : :);
    }
#else
    static void relax() noexcept
    {
    }
#endif
};

}

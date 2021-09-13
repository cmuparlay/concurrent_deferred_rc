/*
 * File: toStringStream.h
 * Copyright 2016, 2017 Terrain Data, Inc.
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

#include <sstream>

namespace terrain
{

template<class ... Args>
static inline std::stringstream toStringStream(Args... args)
{
    std::stringstream s;
    (void)(int[])
    {
        0, ((void)(s << args), 0) ...
    };
    return s;
}

}

/*
 * File: DebugPrintf.h
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

#include "types.h"
#include "toStringStream.h"


namespace terrain
{

/**
 * A command line debug print utility function
 */

/**
 * prints a debug message
 * @param message sprintf formatted message string
 * @param ... any values to place in the message (sprintf format)
 */
extern void dprint(char const* message, ...);

/**
 * prints a debug message
 * @param s message to print
 */
extern void dprint(string s);

template<class ... Args>
static inline void dout(Args ... args)
{
    dprint(toStringStream(args..., "\n").str());
}

}

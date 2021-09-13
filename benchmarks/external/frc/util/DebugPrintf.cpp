/*
 * File: DebugPrintf.cpp
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

#include <string>
#include <sstream>
#include <stdarg.h>
#include <iostream>
#include <stdio.h>

#include <util/DebugPrintf.h>

#include "util.h"

namespace terrain
{

void dprint(char const* message, ...)
{
    sz const bufferLength = 4096;
    sz const maxLength = bufferLength - sizeof(char);
    char buffer[bufferLength];

    va_list argptr;
    va_start(argptr, message);
    vsnprintf(buffer, maxLength, message, argptr);
    va_end(argptr);

    std::cout << buffer;
}

void dprint(string s)
{
    std::cout << s;
}

} /* namespace terrain */

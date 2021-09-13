/*
 * File: Exception.cpp
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

#include <stdarg.h>
#include <stdio.h>

#include "Exception.h"

namespace terrain
{

Exception::Exception(const char* description, ...)
{
    sz const maxLength = 4096 - sizeof(sz) * 2;
    char buffer[maxLength];

    va_list argptr;
    va_start(argptr, description);
    vsnprintf(buffer, maxLength, description, argptr);
    va_end(argptr);
    this->description = string(buffer);
}

Exception::Exception(string description) :
    description(std::move(description))
{
}

Exception::Exception()
{
}

Exception::~Exception() noexcept
{
}

string const& Exception::getDescription() const
{
    return description;
}

void Exception::setDescription(string& s)
{
    description = s;
}

} // namespace terrain

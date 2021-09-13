/*
 * File: ArrayHeader.h
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

#include "ObjectHeader.h"

namespace terrain
{
namespace frc
{
namespace detail
{

class ArrayHeader
{
public:

    explicit ArrayHeader(intt count, uint typeCode, sz length_) noexcept :
        length_(length_),
        objectHeader(count, typeCode)
    {
        ;
    }

    ArrayHeader(ArrayHeader&& source) = delete;
    ArrayHeader(ArrayHeader const& source) = delete;
    ArrayHeader& operator=(ArrayHeader&& source) = delete;
    ArrayHeader& operator=(ArrayHeader const& source) = delete;

    void* getArray() const noexcept
    {
        return objectHeader.getObject();
    }

    sz length() const noexcept
    {
        return length_;
    }

private:
    sz length_;
    ObjectHeader objectHeader;
};


} /* namespace detail */
} /* namespace frc */
} /* namespace terrain */

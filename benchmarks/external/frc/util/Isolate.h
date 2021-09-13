/*
 * File: Isolate.h
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

#include "atomic.h"


namespace terrain
{

/**
 * A wrapper for padding an element out to consume an integral number of
 * cache lines. This can be used to reduce false-sharing between data
 * members when they are being concurrently accessed by more than one
 * processor.
 *
 * Presents a pointer-like interface to the member, as well as a
 * get() function which returns a reference to the wrapped element.
 */
template<class T>
class Isolate
{
private:

    static constexpr sz cacheLinesPerElement = (sizeof(T) / cacheLineSize);
    static constexpr sz paddingSize = cacheLineSize
                                      - (sizeof(T) - (cacheLineSize* cacheLinesPerElement));

private:
    T element;
    byte buffer[paddingSize];

public:

    template<typename ... Args>
    inline Isolate(Args&& ... args)
        :
        element(args...) { }

    inline ~Isolate() { }

    T& get()
    {
        return element;
    }

    inline T* operator->()
    {
        return &element;
    }

    inline T& operator*()
    {
        return element;
    }

    inline T* asPtr()
    {
        return &element;
    }
};

template<typename T>
using AIsolate = Isolate<atm <T>>;

}

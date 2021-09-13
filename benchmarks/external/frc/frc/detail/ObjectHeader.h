/*
 * File: ObjectHeader.h
 * Copyright 2015-2018 Terrain Data, Inc.
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

#include <cstdio>
#include <type_traits>
#include <typeinfo>
#include <memory>

#include "../../util/util.h"
#include "../../util/DebugPrintf.h"

#include "FRCConstants.h"
#include "DestructorMap.h"

namespace terrain
{
namespace frc
{
namespace detail
{

class ArrayHeader;

class ObjectHeader;

static ObjectHeader* getObjectHeader(void const* object) noexcept;

static ArrayHeader* getArrayHeader(void const* array) noexcept;

static ArrayHeader* getArrayHeader(ObjectHeader const* objectHeader) noexcept;

template<typename T, typename ... Args>
static T* makeNewObject(uint count, Args&& ... args);

template<typename T>
static T* makeNewArray(uint count, sz length_, bool initialize);

class ObjectHeader
{
private:
    static constexpr bool debug = false;

    friend class ArrayHeader;

public:

    explicit ObjectHeader(uint count_, uint typeCode) noexcept :
        count(count_),
        typeCode(typeCode)
    {
        ;
    }

    ObjectHeader(ObjectHeader&& source) = delete;

    ObjectHeader(ObjectHeader const& source) = delete;

    ObjectHeader& operator=(ObjectHeader&& source) = delete;

    ObjectHeader& operator=(ObjectHeader const& source) = delete;

    void* getObject() const noexcept
    {
        return (void*)(((intptr_t) this) + sizeof(ObjectHeader));
    }

    void increment() noexcept
    {
        count.fetch_add(1, oarl);
    }

    /**
     * Decrements the count atomically, unless it would reach zero.
     * @return true if count was decremented, false if count was 1
     */
    bool tryDecrement() noexcept
    {
        if(!FRCConstants::enableSemiDeferredDecrements)
            return false;

        auto c = count.load(orlx);
        for(;;)
        {
            if(c <= 1)
                return false;

            if(count.compare_exchange_weak(c, c - 1, oarl, oacq))
            {
                return true;
            }
        }
    }

    /**
     * Decrements the count atomically.
     * If count would reach zero, just returns true instead.
     *
     * TODO:: check this against fetch_sub()
     * @return true if count is <= 1, false if it wasn't and was decremented
     */
    bool decrement() noexcept
    {
        if(FRCConstants::enableCheckedDecrements)
            return count.load(ocon) <= 1 || count.fetch_sub(1, oarl) <= 1;
        else
            return count.fetch_sub(1, oarl) <= 1;
    }

    void decrementAndDestroy() noexcept
    {
        if(decrement())
            destroy();
    }

    bool isObject() const noexcept
    {
        return (typeCode & 1) == 0;
    }

    bool isArray() const noexcept
    {
        return (typeCode & 1) != 0;
    }

    sz length() const noexcept;

    void destroy() noexcept
    {
        DestructorMap::callDestructor(this, typeCode);
    }

    sz getCount() const noexcept
    {
        return count.load(oacq);
    }

public:
    atm<uint> count;
    uint const typeCode;

};


}
}
}

#include "ArrayHeader.h"

namespace terrain
{
namespace frc
{
namespace detail
{

static ObjectHeader* getObjectHeader(void const* object) noexcept
{
    return (ObjectHeader*)((size_t) object - sizeof(ObjectHeader));
}

static ArrayHeader* getArrayHeader(void const* array) noexcept
{
    return (ArrayHeader*)((size_t) array - sizeof(ArrayHeader));
}

static ArrayHeader* getArrayHeader(ObjectHeader const* objectHeader) noexcept
{
    return getArrayHeader(objectHeader->getObject());
}

inline sz ObjectHeader::length() const noexcept
{
    if(isObject())
        return 1;
    ArrayHeader* arrayHeader = getArrayHeader(this);
    return arrayHeader->length();
}

template<typename T, typename ... Args>
static T* makeNewObject(uint count, Args&& ... args)
{
    /* we need to call getTypeCode() to avoid static initialization order issues.
     * makeNew is called before TypeCodeInitializer<T>::typeCode is initialized
     */
    static auto const typeCode = DestructorMap::getTypeCode<T>();

    static constexpr auto size = sizeof(ObjectHeader) + sizeof(T);
    auto mem = malloc(size); //allocate memory
    if(mem == nullptr)
        throw std::bad_alloc();

    auto const header = new(mem)ObjectHeader(count, typeCode); //place header

    try
    {
        return new(header->getObject())T(std::forward<Args>(args)...); //place object
    }
    catch(...)
    {
        free(mem); //ok to not destruct header
        throw;
    }
}

/**
 * Object Destructor thunk.
 * Calls the given object's destructor.
 */
template<class T>
static void destroyObject(ObjectHeader* header)
{
    auto object = (T*) header->getObject();

    try
    {
        object->~T();
    }
    catch(...)
    {
        assert(false); //Destructors should never throw.
        //absorb
    }

    free(header); //ok to not destruct header
}

template<typename T>
static T* makeNewArray(uint count, sz length, bool initialize)
{
    auto size = sizeof(ArrayHeader) + sizeof(T) * length;
    auto mem = malloc(size); //allocate memory
    if(mem == nullptr)
        throw std::bad_alloc();

    // we still need to call getTypeCode() to avoid static initialization order issues
    static auto const typeCode = DestructorMap::getArrayTypeCode<T>();

    // (e.gh makeNew is called before TypeCodeInitializer<T>::typeCode is initialized)
    auto header = new(mem)ArrayHeader(count, typeCode, length); //place header

    try
    {
        T* array = (T*)(header->getArray());     //get array pointer

        if(initialize)
        {
            sz i = 0;
            try
            {
                for(; i < length; ++i)
                    new(array + i) T();
            }
            catch(...)
            {
                //try to destroy partially constructed members
                while(i > 0)
                    array[--i].~T();

                throw;
            }
        }

        return array;
    }
    catch(...)
    {
        free(mem); //ok to not destruct header
        throw;
    }
}

/**
 * Array Destructor thunk.
 * Calls the given object's destructor.
 */
template<class T>
static void destroyArray(ObjectHeader* objectHeader)
{
    T* array = (T*) objectHeader->getObject();
    auto arrayHeader = getArrayHeader(objectHeader);
    auto length = arrayHeader->length();

    for(sz i = 0; i < length; ++i)
    {
        try
        {
            array[i].~T();
        }
        catch(...)
        {
            assert(false); //Destructors should never throw.
            //absorb
        }
    }

    free(arrayHeader); //ok to not destruct header
}

}
}
}

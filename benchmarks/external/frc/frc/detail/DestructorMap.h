/*
 * File: DestructorMap.h
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

#pragma once

#include <vector>
#include <typeinfo>
#include <typeindex>
#include <unordered_map>
#include <cassert>

#include "../../util/types.h"

namespace terrain
{
namespace frc
{
namespace detail
{

class DestructorMap;
class ObjectHeader;

template<class T>
static void destroyObject(ObjectHeader* header);

template<class T>
static void destroyArray(ObjectHeader* objectHeader);

// We use a function to retrieve the destructor map to avoid static initialization ordering issues
DestructorMap& getDestructorMap(); //defined in FRCManager.cpp

class DestructorMap
{
public:
    using Destructor = void(*)(ObjectHeader* header);

private:

    static constexpr sz initialCapacity = 8192;

public:

    DestructorMap()
    {
        destructors.reserve(initialCapacity);
    }

    ~DestructorMap()
    {
        ;
    }

    template<class T>
    static uint getTypeCode()
    {
        //TODO: investigate better ways of doing this.
        static auto const typeCode = getDestructorMap().registerType<T>();
        return typeCode;
    }

    template<class T>
    static uint getArrayTypeCode()
    {
        //TODO: investigate better ways of doing this.
        static auto const typeCode = getTypeCode<T>() + 1;
        return typeCode;
    }

    static void callDestructor(ObjectHeader* header, uint typeCode)
    {
        static auto& dm = getDestructorMap();

        assert(typeCode < dm.destructors.size());
        auto destructor = dm.destructors[typeCode];
        assert(destructor != nullptr);

        destructor(header);
    }

private:

    template<class T>
    uint registerType()
    {
        static_assert(
            sizeof(T*) == sizeof(void*),
            "Type pointer and void pointer must be the same size.");

        std::type_index typeIndex(typeid(T));
        auto iter = typeIDToTypeCodeMap.find(typeIndex);
        if(iter != typeIDToTypeCodeMap.end())
            return iter->second; //hit

        //miss
        uint typeCode = (uint)destructors.size();
        typeIDToTypeCodeMap.insert(iter, {typeIndex, typeCode});
        destructors.emplace_back(&destroyObject<T>);
        destructors.emplace_back(&destroyArray<T>);

        return typeCode;
    }

private:
    std::vector<Destructor> destructors;
    std::unordered_map<std::type_index, uint> typeIDToTypeCodeMap; //TODO: optimize
};

}
}
}

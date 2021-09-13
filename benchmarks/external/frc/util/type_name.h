/*
 * File: type_name.h
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

#include <cstdlib>
#include <typeinfo>
#include <string>

#include "platformSpecific.h"
#include "types.h"

#ifdef TERRAIN_GCC
#include <cxxabi.h>
#endif

namespace terrain
{

/**
 * Debugging utility for getting a string that contains the type name of
 * a particular type. Useful for certain template-based debugging applications
 * where the classname is not easily accessible.
 */
template<class T>
std::string type_name()
{
    std::string tname = typeid(T).name();

#ifdef TERRAIN_GCC
    int status;
    char* demangled_name = abi::__cxa_demangle(tname.c_str(), NULL, NULL, &status);
    if(status == 0)
    {
        tname = demangled_name;
        std::free(demangled_name);
    }
#endif

    return tname;
}

}

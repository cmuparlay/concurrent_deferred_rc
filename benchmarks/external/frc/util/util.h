/*
 * File: util.h
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

/**
 * Includes the most useful definitions and functions in the util dir.
 */

#include <cassert>
#include "defs.hpp"
#include "directives.h"
#include "type_name.h"
#include "DebugPrintf.h"

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

namespace terrain
{

template<class T>
using rr = std::remove_reference_t<T>; //deprecated

}

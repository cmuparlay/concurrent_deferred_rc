/*
 * File: tlsGCC.h
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

#include "platformSpecific.h"

/**
 * GCC implementation / aliases for thread local storage
 */

#if (defined TERRAIN_GCC)

#define tls(type,name) __thread type name

#endif

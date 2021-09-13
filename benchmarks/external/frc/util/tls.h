/*
 * File: tls.h
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

#include "platformSpecific.h"

/**
 * Supplies simple thread-local storage. Typically more efficient than
 * std::thread_local.
 *
 * Defines the preprocessor directive tls(type, name) which declares a
 * thread local variable with the given type and name.
 *
 * Only use this for small primitive types like pointers and integers.
 * The more TLS entries that have to be maintained, the slower TLS access
 * becomes, so try not to use too many of these!
 *
 * For GCC, this aliases to __thread type name.
 */

#ifdef TERRAIN_GCC
#include "tlsGCC.h"
#else
#error "Platform not supported."
#endif

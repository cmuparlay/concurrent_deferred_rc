/*
 * File: directives.h
 * Copyright 2015, 2017 Terrain Data, Inc.
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
 * Defines several compiler directives for supporting compilers
 */

#include "platformSpecific.h"

/**
 * The restrict directive lets the compiler make optimizations to memory
 * accesses by assuming that no other pointer accesses in the same scope
 * write to memory accessed by dereferencing this pointer.
 *
 * Essentially the same as the C keyword restrict.
 *
 * see: https://gcc.gnu.org/onlinedocs/gcc/Restricted-Pointers.html
 */

#ifndef __cplusplus
#define restrict_ restrict
#elif defined(TERRAIN_MSVC)
#define restrict_ __restrict
#elif defined(TERRAIN_GCC)
#define restrict_ __restrict__
#else
#define restrict_
#endif

/**
 * Don't use these
 */
#if defined(TERRAIN_MSVC)
#define assume_aligned_(x,alignment)
#define aligned_(alignment) __declspec( align( alignment ) )
#elif defined(TERRAIN_GCC)
#define assume_aligned_(x,alignment) __builtin_assume_aligned(x,alignment,0)
#define aligned_(alignment) __attribute__ ((__aligned__(alignment)))
#else
#define assume_aligned_(x,alignment)
#define aligned_(alignment)
#endif


#undef unlikely
#undef likely

/**
 * These hints can be used to tell the compiler a condition is very likely
 * or unlikely to be true. Don't use this without performance testing, and
 * only when it makes a big, useful different. Generally the expectation
 * needs to be true 9999 out of 10000 times before this is faster than without
 * the hint.
 */
#if defined(TERRAIN_GCC)
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(expr) (expr)
#define unlikely(expr) (expr)
#endif


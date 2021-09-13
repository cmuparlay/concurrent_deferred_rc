/*
 * File: types.h
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

#include <ostream>
#include <limits>
#include <string>
#include <cstdint>

/**
 * Defines the terrain primitive types.
 */

namespace terrain
{
/**
 * A signed byte
 */
using sbyte = std::int8_t;

/**
 * An unsigned byte
 */
using byte = std::uint8_t;

/**
 * A signed 2-byte, 16-bit integer a.k.a "a short"
 */
using sht = std::int16_t;

/**
 * An unsigned 2-byte, 16-bit integer a.k.a "an unsigned short"
 */
using usht = std::uint16_t;

/**
 * A signed 4-byte, 32-bit integer a.k.a "an int"
 */
using intt = std::int32_t;

/**
 * An unsigned 4-byte, 32-bit integer a.k.a "an unsigned int"
 */
using uint = std::uint32_t;

/**
 * A signed 8-byte, 64-bit integer a.k.a "a long"
 */
using lng = std::int64_t;

/**
 * An unsigned 8-byte, 64-bit integer a.k.a "an unsigned long"
 */
using ulng = std::uint64_t;

/**
 * An unsigned integer with the same size as a data pointer like void*.
 * On 64-bit gcc, targeting x64, this is the same as a ulng: a
 * 64-bit unsigned integer.
 *
 * size_t and sz should be used for counting things, especially things
 * like array indexes and numbers of bytes. They are also what you get
 * back when subtracting pointers.
 *
 * If you need to access the value in a pointer as an integer, this is
 * the type you should cast it to.
 */
using sz = std::size_t;

/**
 * A native signed integer type which has the greatest capacity without
 * sacrificing arithmetic performance, a.k.a "a num".
 *
 * In x64 systems this is the same as a lng.
 */
using nm = std::int_fast64_t;

/**
 * A native unsigned integer type which has the greatest capacity without
 * sacrificing arithmetic performance, a.k.a "an unsigned num".
 *
 * In x64 systems this is the same as a ulng.
 */
using un = std::uint_fast64_t;

/**
 * Terrain's common string class. The fact that it is std::string is beyond the point.
 */
using string = std::string;

} // namespace terrain

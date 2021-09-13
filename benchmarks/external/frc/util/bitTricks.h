/*
 * File: bitTricks.h
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
#include "types.h"

#include <cassert>

/**
 * Utility functions for bit manipulation.
 * Many of these tricks are inspired by the
 * Stanford Bit Twiddling Hacks, which can be found
 * here: https://graphics.stanford.edu/~seander/bithacks.html
 * Such code is in the public domain according to the above link.
 */


namespace terrain
{

/**
 * Checks if the source value will fit in the destination type.
 * @return true if the source value equals the destination value
 */
template<class Destination, class Source>
bool fits(Source value)
{
    return (Source)(Destination)value == value;
}

/**
 * Checks if two source values will fit in the destination type.
 * @return true if both source values equals their destination value
 */
template<class Destination, class Source>
bool fits(Source v1, Source v2)
{
    return fits<Destination>(v1) && fits<Destination>(v2);
}

/**
 * Performs a logical right shift on x
 */
template<typename T>
inline static T logicalRightShift(T const x, T const n)
{
    return (unsigned)x >> n;
}

/**
 * Performs an arithmetic right shift on x, which is equivalent to an integer
 * division by 2^n
 */
template<typename T>
inline static T arithmeticRightShift(T const x, T const n)
{
    if(x < 0 && n > 0)
        return x >> n | ~(~0U >> n);
    else
        return x >> n;
}

/**
 * Rotates the bits of value to the left by shift number of bits
 */
template<typename T, typename S>
inline static T rotl(T value, S shift)
{
#ifndef NDEBUG
    // in debug mode (with -fsanitize) make sure we don't shift too much
    if(shift == 0 || shift == (sizeof(value) * 8))
    {
        return value;
    }
#endif

    return (value << shift) | (value >> (sizeof(value) * 8 - shift));
}

/**
 * Rotates the bits of value to the right by shift number of bits
 */
template<typename T, typename S>
inline static T rotr(T value, S shift)
{
    return (value >> shift) | (value << (sizeof(value) * 8 - shift));
}

/**
 * @return the population count (number of set bits) in v
 */
inline static uint popcountByte(uint v)
{
    v = v - ((v >> 1) & 0x55);
    v = (v & 0x33) + ((v >> 2) & 0x33);
    return (v + (v >> 4)) & 0x0F;
}

/**
 * @return the population count (number of set bits) in v
 */
inline static uint popcount(uint v)
{
#if defined TERRAIN_X64 && defined TERRAIN_GCC && defined __SSE4_2__
    uint result;
    asm(

        "xorl %0, %0;\n" //required due to bug in Intel implementation of popcnt
        "popcntl %1, %0;\n"
        : "=&r"(result)
        : "r"(v)
        : "cc");
    return result;
#else
    v = v - ((v >> 1) & 0x55555555); // reuse input as temporary
    v = (v & 0x33333333) + ((v >> 2) & 0x33333333); // temp
    return (((v + (v >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24; // count
#endif
}

/**
 * @return the population count (number of set bits) in v
 */
inline static ulng popcount(ulng x)
{
#if defined TERRAIN_X64 && defined TERRAIN_GCC && defined __SSE4_2__
    ulng result;
    asm(
        "xorq %0, %0;\n" //required due to bug in Intel implementation of popcnt
        "popcntq %1, %0;\n"
        : "=&r"(result)
        : "r"(x)
        : "cc");
    return result;
#else
    static constexpr ulng k1 = ulng(0x5555555555555555ULL); /*  -1/3   */
    static constexpr ulng k2 = ulng(0x3333333333333333ULL); /*  -1/5   */
    static constexpr ulng k4 = ulng(0x0f0f0f0f0f0f0f0fULL); /*  -1/17  */
    static constexpr ulng kf = ulng(0x0101010101010101ULL); /*  -1/255 */

    x = x - ((x >> 1) & k1); /* put count of each 2 bits into those 2 bits */
    x = (x & k2) + ((x >> 2) & k2); /* put count of each 4 bits into those 4 bits */
    x = (x + (x >> 4)) & k4; /* put count of each 8 bits into those 8 bits */
    x = (x * kf) >>
        56; /* returns 8 most significant bits of x + (x<<8) + (x<<16) + (x<<24) + ...  */
    return x;
#endif
}

/**
 * v must be an integer.
 * @return true if v is a power of two
 */
template<typename T>
inline static bool isPowerOfTwo(T v)
{
    static_assert(std::is_integral<T>::value,
                  "isPowerOfTwo() can only be used with integral types.");

    return v && !(v & (v - 1));
}

/**
 * constexpr version of isPowerOfTwo() which also returns true if
 * v is zero. May be slightly faster than isPowerOfTwo().
 *
 * v must be an integer.
 * @return true if v is a power of two, or zero
 */
template<typename T>
inline static constexpr bool isPowerOfTwoOrZero(T v)
{
    static_assert(std::is_integral<T>::value,
                  "isPowerOfTwoOrZero() can only be used with integral types.");

    return (v & (v - 1)) == 0;
}

/**
 * Counts the number of trailing zeros in v. A trailing zero
 * is a zero after the last (least significant) set bit.
 * @return the number of trailing zeros in v
 */
inline static byte countTrailingZeros(ulng v)
{
#if defined TERRAIN_X64 && defined TERRAIN_GCC
    return (v == 0) ? 64 : __builtin_ctzll(v);
#endif

    if(v == 0)
        return 64;
    v &= -(lng)(v);

    byte a = (v & 0x5555555555555555ULL) != 0;
    byte b = 63;

    if(v & 0x3333333333333333ULL)
        a += 2;
    if(v & 0x0F0F0F0F0F0F0F0FULL)
        b -= 4;

    if(v & 0x00FF00FF00FF00FFULL)
        a += 8;
    if(v & 0x0000FFFF0000FFFFULL)
        b -= 16;
    if(v & 0x00000000FFFFFFFFULL)
        a += 32;

    return b - a;
}

/**
 * Counts the number of trailing zeros in v. A trailing zero
 * is a zero after the last (least significant) set bit.
 * @return the number of trailing zeros in v
 */
inline static byte countTrailingZeros(uint v)
{
#if defined TERRAIN_X64 && defined TERRAIN_GCC
    return (v == 0) ? 32 : __builtin_ctz(v);
#endif

    if(v == 0)
        return 32;

    unsigned int c = 32; // c will be the number of zero bits on the right
    v &= -(signed) v; // get trailing set bit
    if(v) c--;
    if(v & 0x0000FFFF) c -= 16;
    if(v & 0x00FF00FF) c -= 8;
    if(v & 0x0F0F0F0F) c -= 4;
    if(v & 0x33333333) c -= 2;
    if(v & 0x55555555) c -= 1;
    return c;

    /* EAM: this wasn't working
    v &= -(int)(v);

    byte a = (v & 0x55555555) != 0;
    byte b = 31;

    if(v & 0x33333333)
    	a += 2;
    if(v & 0x0F0F0F0F)
    	b -= 4;

    if(v & 0xFF00FF00)
    	a += 8;
    if(v & 0xFFFF0000)
    	b -= 16;

    return b - a;
    */
}

/**
 * Counts the number of trailing zeros in v. A trailing zero
 * is a zero after the last (least significant) set bit.
 * @return the number of trailing zeros in v
 */
inline static byte countTrailingZeros(usht v)
{
    if(v == 0)
        return 16;

    v &= -(sbyte)(v);

    byte a = ((v & 0x5555) != 0);
    byte b = 2 * ((v & 0x3333) != 0);
    byte c = 4 * ((v & 0x0F0F) != 0);
    byte d = 8 * ((v & 0x00FF) != 0);

    return 15 - a - b - c - d;
}

/**
 * Counts the number of trailing zeros in v. A trailing zero
 * is a zero after the last (least significant) set bit.
 * @return the number of trailing zeros in v
 */
inline static byte countTrailingZeros(byte v)
{
    if(v == 0)
        return 8;

    v &= -(sbyte)(v);

    byte a = ((v & 0x55) != 0);
    byte b = 2 * ((v & 0x33) != 0);
    byte c = 4 * ((v & 0x0F) != 0);

    return 7 - a - b - c;
}

/**
 * computes floor(log2(v)) fairly efficiently
 */
inline static byte floorLog2(byte v)
{
    sz r = 0;
    sz shift;

    shift = (v > 0xF) << 2;
    v >>= shift;
    r |= shift;

    shift = (v > 0x3) << 1;
    v >>= shift;
    r |= shift;

    r |= (v >> 1);

    return r;
}

inline static ulng countLeadingZeros(ulng v);
inline static uint countLeadingZeros(uint v);

/**
 * computes floor(log2(v)) fairly efficiently
 */
inline static uint floorLog2(uint v)
{
#if defined TERRAIN_X64 && defined TERRAIN_GCC
    return v == 0 ? 0 : (31 - countLeadingZeros(v));
#else
    uint r;
    uint shift;

    shift = (v > 0xFFFF) << 4;
    v >>= shift;
    r |= shift;

    shift = (v > 0xFF) << 3;
    v >>= shift;
    r |= shift;

    shift = (v > 0xF) << 2;
    v >>= shift;
    r |= shift;

    shift = (v > 0x3) << 1;
    v >>= shift;
    r |= shift;

    r |= (v >> 1);

    return r;
#endif
}

/**
 * computes floor(log2(v)) fairly efficiently
 */
inline static ulng floorLog2(ulng v)
{
#if defined TERRAIN_X64 && defined TERRAIN_GCC
    return v == 0 ? 0 : (63 - countLeadingZeros(v));
#else
    ulng r;
    ulng shift;

    r = (v > 0xFFFFFFFFULL) << 5;
    v >>= r;

    shift = (v > 0xFFFF) << 4;
    v >>= shift;
    r |= shift;

    shift = (v > 0xFF) << 3;
    v >>= shift;
    r |= shift;

    shift = (v > 0xF) << 2;
    v >>= shift;
    r |= shift;

    shift = (v > 0x3) << 1;
    v >>= shift;
    r |= shift;

    r |= (v >> 1);

    return r;
#endif
}

/**
 * computes ceiling(log2(v)) fairly efficiently
 */
inline static sz ceilLog2(sz v)
{
    //return floorLog2(roundUpToPowerOfTwo(v));
    return v == 0 ? 0 : (floorLog2(v - 1) + 1);
}

/**
 * Counts the number of leading zeros in v. A leading zero
 * is a zero before the first (most significant) set bit.
 * @return the number of leading zeros in v
 */
inline static uint countLeadingZeros(uint v)
{
    if(v == 0)
        return 32;

#if defined TERRAIN_X64 && defined TERRAIN_GCC
    return __builtin_clz(v);
    //	uint result;
    //	asm(
    //			"xorl %0, %0;\n"
    //			"lzcntl %1, %0;\n"
    //			: "=&r" (result)
    //			: "r" (v)
    //			: "cc");
    //	return result;
#else
    return (31 - floorLog2(v));
#endif
}

/**
 * Counts the number of leading zeros in v. A leading zero
 * is a zero before the first (most significant) set bit.
 * @return the number of leading zeros in v
 */
inline static ulng countLeadingZeros(ulng v)
{
    if(v == 0)
        return 64;

#if defined TERRAIN_X64 && defined TERRAIN_GCC
    return __builtin_clzll(v);
    //	ulng result;
    //	asm(
    //			"xorq %0, %0;\n"
    //			"lzcntq %1, %0;\n"
    //			: "=&r" (result)
    //			: "r" (v)
    //			: "cc");
    //	return result;
#else
    return (63 - floorLog2(v));
#endif
}



/**
 * computes the largest power of 2 that evenly divides a number
 */
template<typename T>
inline static constexpr T greatestPowerOf2Factor(T v)
{
    //http://stackoverflow.com/questions/1551775/calculating-highest-power-of-2-that-evenly-divides-a-number-in-c
    //works by clearing all bits except the trailing one bit
    return v & ~v;
}

/**
 * rounds a number up to the nearest power of two
 */
inline static usht roundUpToPowerOfTwo(usht v)
{
    if(v == 0)
        return 1;
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v++;
    return v;
}

/**
 * rounds a number up to the nearest power of two
 */
inline static uint roundUpToPowerOfTwo(uint v)
{
#ifdef TERRAIN_GCC
    return uint(1) << ceilLog2(v);
#else
    if(v == 0)
        return 1;
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
#endif
}

/**
 * rounds a number up to the nearest power of two
 */
inline static sz roundUpToPowerOfTwo(sz v)
{
#ifdef TERRAIN_GCC
    return uint(1) << ceilLog2(v);
#else
    if(v == 0)
        return 1;
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    v++;
    return v;
#endif
}

/**
 * rounds a number up to the nearest power of two
 */
inline static nm roundUpToPowerOfTwo(nm v)
{
    return (nm)roundUpToPowerOfTwo((sz)v);
}

/**
 * rounds a down up to the nearest power of two
 */
template<typename T>
inline static T roundDownToPowerOfTwo(T v)
{
    return roundUpToPowerOfTwo((T)(v / 2 + 1));
}

/**
 * constexpr that computes floor(log2(value))
 */
template<typename T>
constexpr inline static T log2Constexpr(T value)
{
    return value <= 1
           ? T(0)
           :
           T(1) + log2Constexpr(
               value / 2);
}

/**
 * constexpr that rounds a number up to the nearest power of two
 */
template<typename T>
constexpr inline static T roundUpToPowerOfTwoConstexpr(T value)
{
    return value == T(1) << log2Constexpr(value) ?
           value :
           2 * (T(1) << log2Constexpr(value));
}

/**
 * constexpr that rounds a number down to the nearest power of two
 */
template<typename T>
constexpr inline static T roundDownToPowerOfTwoConstexpr(T value)
{
    return value <= 0 ? T(0) : (T(1) << log2Constexpr(value));
}

/**
 * efficiently computes ceil(numerator / divisor) as
 * long as numerator + divisor does not overflow and
 * as long as both numbers are positive.
 */
template<typename T>
constexpr inline static T ceilPositiveNoOverflow(T numerator, T divisor)
{
    return (numerator + divisor - 1) / divisor;
}

/**
 * Gives ceil((double)numerator / denominator) without
 * resurting to floating point calculations.
 *
 * Only works with non-negative integers!
 */
template<typename T>
constexpr inline static T ceilDivision(T numerator, T denominator)
{
    //return (numerator + denominator - 1) / denominator;

    //x86 gives you remainder for free so a good compiler will make this fast
    return numerator / denominator + (numerator % denominator != 0);
}


} /* namespace terrain */

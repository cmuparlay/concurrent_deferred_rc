/*
 * File: platformSpecific.h
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

/**
 * This header sets several preprocessor directives used throughout CTools to
 * conditionally compile compiler or machine specific code.
 */

//processor types
#if defined __x86_64__ //&& defined __MMX__ && defined __SSE__ && defined __SSE2__ && defined __SSE3__ && defined __SSE4_2__
#define TERRAIN_X64
#define TERRAIN_LITTLE_ENDIAN
#else
#error "Target processor not supported."
#endif

//compilers
#if defined __GNUG__
#define TERRAIN_GCC
#elif defined _MSC_VER
#define TERRAIN_MSVC
#else
#error "Compiler not Supported."
#endif

//platforms
//#if ((defined _WIN32) )//|| (defined __CYGWIN__) || (defined __MINGW32__))
#ifdef _WIN32
#define TERRAIN_WINDOWS
#elif defined __FreeBSD__
#define TERRAIN_FREEBSD
#elif defined __linux__
#define TERRAIN_LINUX
#else
#error "Platform not Supported."
#endif

//compiler x platform combinations
#if defined TERRAIN_GCC
#ifdef TERRAIN_WINDOWS
#define TERRAIN_WINDOWSGCCx64
//		#define NTDDI_VERSION NTDDI_VISTA
#elif defined TERRAIN_FREEBSD
#define TERRAIN_BSDGCCx64
#elif defined TERRAIN_LINUX
#define TERRAIN_LINUXGCCx64
#endif
#else
#error "(Compiler, Platform) combination not Supported."
#endif

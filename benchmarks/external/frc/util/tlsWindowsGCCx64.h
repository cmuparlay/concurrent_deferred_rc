/*
 * File: tlsWindowsGCCx64.h
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

#ifdef TERRAIN_WINDOWSGCCx64

/**
 * Windows-based Mingw-GCC implementation for thread local storage.
 *
 * Uses windows API calls TlsAlloc(), TlsFree(), and handles the rest
 * using assembler calls (which have tested to be considerably faster than
 * the Windows calls).
 */

#include <windows.h>
#include <winnt.h>

namespace terrain
{
namespace detail
{
template<typename T>
struct tls_s
{
private:
    DWORD const offset;

    //	typedef struct _TEB
    //	{
    //		BYTE Reserved1[1952];
    //		PVOID Reserved2[412];
    //		PVOID TlsSlots[64];
    //		BYTE Reserved3[8];
    //		PVOID Reserved4[26];
    //		PVOID ReservedForOle;
    //		PVOID Reserved5[4];
    //		PVOID TlsExpansionSlots;
    //	} TEB, *PTEB;

public:

    tls_s() :
        offset(TlsAlloc())
    {
    }

    ~tls_s()
    {
        TlsFree(offset);
    }

    T operator=(T value)
    {
        set(value);
        return value;
    }

    operator T() const
    {
        return get();
    }

private:
    inline T get() const
    {
        //return (T)TlsGetValue(offset);
        //		return (T)((_TEB*) __readgsqword(FIELD_OFFSET(NT_TIB,Self)))->TlsSlots[offset];
        //return (T)TlsGetValue(offset);
        //		printf("FIELD_OFFSET(_TEB,tlsSlots[0]) = %p\n", (void*)FIELD_OFFSET(_TEB,TlsSlots[0]));
        //		return (T)((_TEB*) __readgsqword(FIELD_OFFSET(NT_TIB,Self)))->TlsSlots[offset];
        T result;
        asm(".intel_syntax \n"
            "movq	%0, %%gs:[0x1480+%1*8]\n"
            ".att_syntax\n"
            : "=r"(result) : "r"(offset));
        return result;
    }

    inline void set(T value)
    {
        //		TlsSetValue(offset, (LPVOID)value);
        //		((_TEB*) __readgsqword(FIELD_OFFSET(NT_TIB,Self)))->TlsSlots[offset] = (PVOID)value;
        asm(".intel_syntax \n"
            "movq	%%gs:[0x1480+%1*8], %0\n"
            ".att_syntax\n"
            : : "r"(value), "r"(offset) : "memory");
    }

    //	//assignment
    //	inline tls& operator=(const tls<T> &r)
    //	{
    //		set(r.get());
    //		return *this;
    //	}
    //
    //	template<typename V>
    //	inline tls& operator=(const V &r)
    //	{
    //		set(&r);
    //		return *this;
    //	}
    //
    //	inline tls& operator=(const T &r)
    //	{
    //		set(&r);
    //		return *this;
    //	}
    //
    ////	inline T& operator*() const
    ////	{
    ////		return get();
    ////	}
    //
    //	inline T* operator->()
    //	{
    //		return get();
    //	}
};
}

#define tls(type,name) ctools::detail::tls_s<type> name

} /* namespace terrain */

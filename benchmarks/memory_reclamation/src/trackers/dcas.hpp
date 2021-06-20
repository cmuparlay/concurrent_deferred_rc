/*
 * Copyright (c) 2019 Ruslan Nikolaev.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

static inline bool dcas_compare_exchange_strong(std::atomic<__uint128_t> &obj,
		__uint128_t & expected, __uint128_t desired,
		std::memory_order succ, std::memory_order fail)
{
#if defined(__x86_64__) && defined(__GNUC__) && !defined(__llvm__) && defined(__GCC_ASM_FLAG_OUTPUTS__)
	uint64_t l = (uint64_t) desired;
	uint64_t h = (uint64_t) (desired >> (sizeof(uint64_t) * 8));
	bool r;
	__asm__ __volatile__ ("lock cmpxchg16b %0"
		  : "+m" (obj), "=@ccz" (r), "+A" (expected)
		  : "b" (l), "c" (h)
	);
	return r;
#else
	return obj.compare_exchange_strong(expected, desired, succ, fail);
#endif
}

static inline bool dcas_compare_exchange_weak(std::atomic<__uint128_t> &obj,
		__uint128_t & expected, __uint128_t desired,
		std::memory_order succ, std::memory_order fail)
{
#if defined(__x86_64__) && defined(__GNUC__) && !defined(__llvm__) && defined(__GCC_ASM_FLAG_OUTPUTS__)
	return dcas_compare_exchange_strong(obj, expected, desired, succ, fail);
#else
	return obj.compare_exchange_weak(expected, desired, succ, fail);
#endif
}

static inline __uint128_t dcas_load(std::atomic<__uint128_t> &obj,
		std::memory_order order)
{
#if defined(__x86_64__) && defined(__GNUC__) && !defined(__llvm__) && defined(__GCC_ASM_FLAG_OUTPUTS__)
	__uint128_t expected = 0;
	dcas_compare_exchange_strong(obj, expected, 0, order, order);
	return expected;
#else
	return obj.load(order);
#endif
}

static inline void dcas_store(std::atomic<__uint128_t> &obj,
		__uint128_t value, std::memory_order order)
{
#if defined(__x86_64__) && defined(__GNUC__) && !defined(__llvm__) && defined(__GCC_ASM_FLAG_OUTPUTS__)
	__uint128_t expected = *((volatile __uint128_t *) (&obj));
	while (!dcas_compare_exchange_weak(obj, expected, value, order, order))
		;
#else
	obj.store(value, order);
#endif
}

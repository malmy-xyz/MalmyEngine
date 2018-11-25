#ifndef SIMDACCEL_H_INCLUDED
#define SIMDACCEL_H_INCLUDED

#include "simddefines.h"

#if defined(SIMD_CPU_ARCH_x86) || defined(SIMD_CPU_ARCH_x86_64)
	#include "x86simdaccel.h"
#else
	#include "simdemulator.h"
#endif

#endif // SIMDACCEL_H_INCLUDED

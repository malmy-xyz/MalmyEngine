#pragma once
#include "simddefines.h"

#if defined(SIMD_CPU_ARCH_x86) || defined(SIMD_CPU_ARCH_x86_64)
	#include "x86simdaccel.h"
#else
	#include "simdemulator.h"
#endif


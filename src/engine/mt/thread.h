#pragma once

#include "engine/malmy.h"

#ifdef __linux__
	#include <pthread.h>
#endif


namespace Malmy { namespace MT {


#ifdef _WIN32
	typedef u32 ThreadID;
#else
	typedef pthread_t ThreadID;
#endif


MALMY_ENGINE_API void setThreadName(ThreadID thread_id, const char* thread_name);
MALMY_ENGINE_API void sleep(u32 milliseconds);
MALMY_ENGINE_API void yield();
MALMY_ENGINE_API u32 getCPUsCount();
MALMY_ENGINE_API ThreadID getCurrentThreadID();
MALMY_ENGINE_API u64 getThreadAffinityMask();


} // namespace MT
} // namespace Malmy

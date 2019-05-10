#pragma once
#include "malmy.h"

namespace Malmy
{

	struct IAllocator;

	namespace JobSystem
	{

		using SignalHandle = u32;
		enum { INVALID_HANDLE = 0xffFFffFF };

		MALMY_ENGINE_API bool init(IAllocator& allocator);
		MALMY_ENGINE_API void shutdown();

		MALMY_ENGINE_API void run(void* data, void(*task)(void*), SignalHandle* on_finish, SignalHandle precondition);
		MALMY_ENGINE_API void wait(SignalHandle waitable);
		MALMY_ENGINE_API inline bool isValid(SignalHandle waitable) { return waitable != INVALID_HANDLE; }

	} // namespace JobSystem

} // namespace Malmy

#pragma once

namespace Malmy
{

	class Engine;

	namespace Fiber
	{

		typedef void* Handle;
		typedef void(__stdcall *FiberProc)(void*);

		constexpr void* INVALID_FIBER = nullptr;

		void initThread(FiberProc proc, Handle* handle);
		Handle create(int stack_size, FiberProc proc, void* parameter);
		void destroy(Handle fiber);
		void switchTo(Handle* from, Handle fiber);

	} // namespace Fiber

} // namespace Malmy

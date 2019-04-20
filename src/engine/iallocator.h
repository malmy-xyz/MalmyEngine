#pragma once 


#ifndef _WIN32
	#include <new>
#endif
#include "engine/malmy.h"


namespace Malmy
{
	struct NewPlaceholder {};
}


inline void* operator new(size_t, Malmy::NewPlaceholder, void* where)
{
	return where;
}


inline void operator delete(void*, Malmy::NewPlaceholder,  void*)
{
}


namespace Malmy
{

#define ALIGN_OF(...) __alignof(__VA_ARGS__)
#define MALMY_NEW(allocator, ...) new (Malmy::NewPlaceholder(), (allocator).allocate_aligned(sizeof(__VA_ARGS__), ALIGN_OF(__VA_ARGS__))) __VA_ARGS__
#define MALMY_DELETE(allocator, var) (allocator).deleteObject(var);


struct MALMY_ENGINE_API IAllocator
{
	virtual ~IAllocator() {}

	virtual void* allocate(size_t size) = 0;
	virtual void deallocate(void* ptr) = 0;
	virtual void* reallocate(void* ptr, size_t size) = 0;

	virtual void* allocate_aligned(size_t size, size_t align) = 0;
	virtual void deallocate_aligned(void* ptr) = 0;
	virtual void* reallocate_aligned(void* ptr, size_t size, size_t align) = 0;

	template <class T> void deleteObject(T* ptr)
	{
		if (ptr)
		{
			ptr->~T();
			deallocate_aligned(ptr);
		}
	}
};


} // namespace Malmy

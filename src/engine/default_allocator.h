#pragma once
#include "engine/iallocator.h"
#include "engine/malmy.h"

namespace Malmy
{

	class MALMY_ENGINE_API DefaultAllocator MALMY_FINAL : public IAllocator
	{
	public:
		void* allocate(size_t n) override;
		void deallocate(void* p) override;
		void* reallocate(void* ptr, size_t size) override;
		void* allocate_aligned(size_t size, size_t align) override;
		void deallocate_aligned(void* ptr) override;
		void* reallocate_aligned(void* ptr, size_t size, size_t align) override;
	};

} // namespace Malmy

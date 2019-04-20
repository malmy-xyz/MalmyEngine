#pragma once

#include "engine/fs/ifile_device.h"
#include "engine/malmy.h"

namespace Malmy
{
	struct IAllocator;

	namespace FS
	{
		struct IFile;

		class MALMY_ENGINE_API MemoryFileDevice MALMY_FINAL : public IFileDevice
		{
		public:
			explicit MemoryFileDevice(IAllocator& allocator) : m_allocator(allocator) {}

			void destroyFile(IFile* file) override;
			IFile* createFile(IFile* child) override;

			const char* name() const override { return "memory"; }

		private:
			IAllocator& m_allocator;
		};
	} // namespace FS
} // namespace Malmy

#pragma once

#include "engine/fs/ifile_device.h"
#include "engine/malmy.h"

struct mf_resource;

namespace Malmy
{
struct IAllocator;

namespace FS
{
struct IFile;

class MALMY_ENGINE_API ResourceFileDevice MALMY_FINAL : public IFileDevice
{
public:
	explicit ResourceFileDevice(IAllocator& allocator)
		: m_allocator(allocator)
	{
	}

	void destroyFile(IFile* file) override;
	IFile* createFile(IFile* child) override;
	int getResourceFilesCount() const;
	const mf_resource* getResource(int index) const;

	const char* name() const override { return "resource"; }

private:
	IAllocator& m_allocator;
};

} // namespace FS

} // namespace Malmy

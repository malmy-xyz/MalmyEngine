#pragma once

#include "engine/fs/ifile_device.h"
#include "engine/fs/os_file.h"
#include "engine/hash_map.h"
#include "engine/malmy.h"


namespace Malmy
{
struct IAllocator;

namespace FS
{
struct IFile;


class MALMY_ENGINE_API PackFileDevice MALMY_FINAL : public IFileDevice
{
	friend class PackFile;
public:
	explicit PackFileDevice(IAllocator& allocator);
	~PackFileDevice();

	IFile* createFile(IFile* child) override;
	void destroyFile(IFile* file) override;
	const char* name() const override { return "pack"; }
	bool mount(const char* path);

private:
	struct PackFileInfo
	{
		u64 offset;
		u64 size;
	};

	HashMap<u32, PackFileInfo> m_files;
	size_t m_offset;
	OsFile m_file;
	IAllocator& m_allocator;
};


} // namespace FS
} // namespace Malmy

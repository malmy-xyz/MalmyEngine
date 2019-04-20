#pragma once

#include "engine/malmy.h"

namespace Malmy
{
namespace FS
{


struct IFile;


struct MALMY_ENGINE_API IFileDevice
{
	IFileDevice() {}
	virtual ~IFileDevice() {}

	virtual IFile* createFile(IFile* child) = 0;
	virtual void destroyFile(IFile* file) = 0;

	virtual const char* name() const = 0;
};


} // namespace FS
} // namespace Malmy

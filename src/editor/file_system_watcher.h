#pragma once

#include "engine/delegate.h"


namespace Malmy
{

struct IAllocator;

class MALMY_EDITOR_API FileSystemWatcher
{
	public:
		virtual ~FileSystemWatcher() {}

		static FileSystemWatcher* create(const char* path, IAllocator& allocator);
		static void destroy(FileSystemWatcher* watcher); 
		virtual Delegate<void (const char*)>& getCallback() = 0;
};


} // namespace Malmy
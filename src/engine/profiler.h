#pragma once


#include "engine/array.h"
#include "engine/malmy.h"
#include "engine/mt/sync.h"
#include "engine/mt/thread.h"


namespace Malmy
{


template <typename T> class DelegateList;


namespace Profiler
{


struct ThreadContext
{
	ThreadContext(IAllocator& allocator) 
		: buffer(allocator)
		, mutex(false)
	{
		buffer.resize(1024 * 512);
	}

	int open_blocks_count = 0;
	Array<u8> buffer;
	uint begin = 0;
	uint end = 0;
	uint rows = 0;
	bool open = false;
	MT::SpinMutex mutex;
	StaticString<64> name;
};


enum class EventType : u8
{
	BEGIN_BLOCK,
	BLOCK_COLOR,
	END_BLOCK,
	FRAME,
	STRING
};


#pragma pack(1)
struct EventHeader
{
	u16 size;
	EventType type;
	u64 time;
};
#pragma pack()


MALMY_ENGINE_API void setThreadName(const char* name);

MALMY_ENGINE_API u64 now();
MALMY_ENGINE_API u64 frequency();
MALMY_ENGINE_API void pause(bool paused);

MALMY_ENGINE_API void beginBlock(const char* name);
MALMY_ENGINE_API void blockColor(u8 r, u8 g, u8 b);
MALMY_ENGINE_API void endBlock();
MALMY_ENGINE_API void frame();
MALMY_ENGINE_API void recordString(const char* value);

MALMY_ENGINE_API void beginFiberSwitch();

MALMY_ENGINE_API Array<ThreadContext*>& lockContexts();
MALMY_ENGINE_API void unlockContexts();

struct Scope
{
	explicit Scope(const char* name) { beginBlock(name); }
	~Scope() { endBlock(); }
};


} // namespace Profiler


#define PROFILE_INT(...)
#define PROFILE_FUNCTION() Profiler::Scope profile_scope(__FUNCTION__);
#define PROFILE_BLOCK(name) Profiler::Scope profile_scope(name);


} // namespace Malmy

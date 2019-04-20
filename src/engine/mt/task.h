#pragma once

#include "engine/malmy.h"


namespace Malmy
{

struct IAllocator;

namespace MT
{


class MALMY_ENGINE_API Task
{
public:
	explicit Task(IAllocator& allocator);
	virtual ~Task();

	virtual int task() = 0;

	bool create(const char* name);
	bool destroy();

	void setAffinityMask(u64 affinity_mask);

	u64 getAffinityMask() const;

	bool isRunning() const;
	bool isFinished() const;
	bool isForceExit() const;

	void forceExit(bool wait);

protected:
	IAllocator& getAllocator();

private:
	struct TaskImpl* m_implementation;
};


} // namespace MT
} // namespace Malmy

#pragma once
#include "engine/malmy.h"

namespace Malmy
{

	MALMY_ENGINE_API u32 crc32(const void* data, int length);
	MALMY_ENGINE_API u32 crc32(const char* str);
	MALMY_ENGINE_API u32 continueCrc32(u32 original_crc, const char* str);
	MALMY_ENGINE_API u32 continueCrc32(u32 original_crc, const void* data, int length);

} // namespace Malmy

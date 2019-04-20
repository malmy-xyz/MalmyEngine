#pragma once

#include "engine/malmy.h"

namespace Malmy
{
namespace MT
{


MALMY_ENGINE_API i32 atomicIncrement(i32 volatile* value);
MALMY_ENGINE_API i32 atomicDecrement(i32 volatile* value);
MALMY_ENGINE_API i32 atomicAdd(i32 volatile* addend, i32 value);
MALMY_ENGINE_API i32 atomicSubtract(i32 volatile* addend,
										i32 value);
MALMY_ENGINE_API bool compareAndExchange(i32 volatile* dest, i32 exchange, i32 comperand);
MALMY_ENGINE_API bool compareAndExchange64(i64 volatile* dest, i64 exchange, i64 comperand);
MALMY_ENGINE_API void memoryBarrier();


} // namespace MT
} // namespace Malmy

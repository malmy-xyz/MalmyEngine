#pragma once

#ifndef _WIN32
	#include <signal.h> // SIGTRAP
#endif

#ifdef _WIN32
	#ifdef _WIN64
		#define PLATFORM64
	#else
		#define PLATFORM32
	#endif
#elif defined(__linux__)
	#ifdef __x86_64__
		#define PLATFORM64
	#else
		#define PLATFORM32
	#endif
#elif defined(__EMSCRIPTEN__)
	#define PLATFORM32
#else
	#error Platform not supported
#endif

#define STRINGIZE_2( _ ) #_
#define STRINGIZE( _ ) STRINGIZE_2( _ )


namespace Malmy
{


typedef char i8;
typedef unsigned char u8;
typedef short i16;
typedef unsigned short u16;
typedef int i32;
typedef unsigned int u32;
typedef unsigned int uint;

#ifdef _WIN32
	typedef long long i64;
	typedef unsigned long long u64;
	#define MALMY_ALIGN_BEGIN(_align) __declspec(align(_align))
	#define MALMY_ALIGN_END(_align)
#elif defined __EMSCRIPTEN__
	typedef long long i64;
	typedef unsigned long long u64;
	#define MALMY_ALIGN_BEGIN(_align)
	#define MALMY_ALIGN_END(_align) __attribute__( (aligned(_align) ) )
#else
	#ifdef PLATFORM64
		typedef long i64;
		typedef unsigned long u64;
	#else
		typedef long long i64;
		typedef unsigned long long u64;
	#endif
	#define MALMY_ALIGN_BEGIN(_align)
	#define MALMY_ALIGN_END(_align) __attribute__( (aligned(_align) ) )
#endif

#ifdef PLATFORM64
	typedef u64 uintptr;
#else
	typedef u32 uintptr;
#endif

static_assert(sizeof(uintptr) == sizeof(void*), "Incorrect size of uintptr");
static_assert(sizeof(i64) == 8, "Incorrect size of i64");
static_assert(sizeof(i32) == 4, "Incorrect size of i32");
static_assert(sizeof(i16) == 2, "Incorrect size of i16");
static_assert(sizeof(i8) == 1, "Incorrect size of i8");

const u32 MAX_PATH_LENGTH = 260;

struct Entity
{
	int index;
	bool operator==(const Entity& rhs) const { return rhs.index == index; }
	bool operator<(const Entity& rhs) const { return rhs.index < index; }
	bool operator>(const Entity& rhs) const { return rhs.index > index; }
	bool operator!=(const Entity& rhs) const { return rhs.index != index; }
	bool isValid() const { return index >= 0; }
};

struct ComponentType
{
	enum { MAX_TYPES_COUNT = 64 };

	int index;
	bool operator==(const ComponentType& rhs) const { return rhs.index == index; }
	bool operator<(const ComponentType& rhs) const { return rhs.index < index; }
	bool operator>(const ComponentType& rhs) const { return rhs.index > index; }
	bool operator!=(const ComponentType& rhs) const { return rhs.index != index; }
};
const ComponentType INVALID_COMPONENT_TYPE = {-1};
const Entity INVALID_ENTITY = {-1};

template <typename T, int count> int lengthOf(const T (&)[count])
{
	return count;
};


} // namespace Malmy


#ifndef ASSERT
	#ifdef NDEBUG
		#define ASSERT(x) { false ? (void)(x) : (void)0; }
	#else
		#ifdef _WIN32
			#define MALMY_DEBUG_BREAK() __debugbreak()
		#else
			#define MALMY_DEBUG_BREAK()  raise(SIGTRAP) 
		#endif
		#define ASSERT(x) do { const volatile bool malmy_assert_b____ = !(x); if(malmy_assert_b____) MALMY_DEBUG_BREAK(); } while (false)
	#endif
#endif

#ifdef _WIN32
	#define MALMY_FINAL final
	#define MALMY_LIBRARY_EXPORT __declspec(dllexport)
	#define MALMY_LIBRARY_IMPORT __declspec(dllimport)
	#define MALMY_FORCE_INLINE __forceinline
	#define MALMY_RESTRICT __restrict
	#define MALMY_ATTRIBUTE_USED
#else 
	#define MALMY_FINAL final
	#define MALMY_LIBRARY_EXPORT __attribute__((visibility("default")))
	#define MALMY_LIBRARY_IMPORT 
	#define MALMY_FORCE_INLINE __attribute__((always_inline)) inline
	#define MALMY_RESTRICT __restrict__
	#define MALMY_ATTRIBUTE_USED __attribute__((used))
#endif

#ifdef STATIC_PLUGINS
	#define MALMY_ENGINE_API
#elif defined BUILDING_ENGINE
	#define MALMY_ENGINE_API MALMY_LIBRARY_EXPORT
#else
	#define MALMY_ENGINE_API MALMY_LIBRARY_IMPORT
#endif

#ifdef STATIC_PLUGINS
	#define MALMY_EDITOR_API
#elif defined BUILDING_EDITOR
	#define MALMY_EDITOR_API MALMY_LIBRARY_EXPORT
#else
	#define MALMY_EDITOR_API MALMY_LIBRARY_IMPORT
#endif

#ifdef STATIC_PLUGINS
	#define MALMY_RENDERER_API
#elif defined BUILDING_RENDERER
	#define MALMY_RENDERER_API MALMY_LIBRARY_EXPORT
#else
	#define MALMY_RENDERER_API MALMY_LIBRARY_IMPORT
#endif

#ifdef _MSC_VER
	#pragma warning(disable : 4251)
	#pragma warning(disable : 4996)
	#if _MSC_VER == 1900 
		#pragma warning(disable : 4091)
	#endif
#endif

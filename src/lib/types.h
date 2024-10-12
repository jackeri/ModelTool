#pragma once

#include <climits>
#include <cstddef>
#include <cstdint>

typedef std::uint8_t u8;
typedef std::uint16_t u16;
typedef std::uint32_t u32;
typedef std::uint64_t u64;

typedef std::int8_t i8;
typedef std::int16_t i16;
typedef std::int32_t i32;
typedef std::int64_t i64;

typedef float f32;
typedef double f64;

typedef bool b8;
typedef std::int32_t b32;

static_assert(sizeof(u8) == 1, "Expected u8 to be 1 byte.");
static_assert(sizeof(u16) == 2, "Expected u16 to be 2 bytes.");
static_assert(sizeof(u32) == 4, "Expected u32 to be 4 bytes.");
static_assert(sizeof(u64) == 8, "Expected u64 to be 8 bytes.");
static_assert(sizeof(i8) == 1, "Expected i8 to be 1 byte.");
static_assert(sizeof(i16) == 2, "Expected i16 to be 2 bytes.");
static_assert(sizeof(i32) == 4, "Expected i32 to be 4 bytes.");
static_assert(sizeof(i64) == 8, "Expected i64 to be 8 bytes.");
static_assert(sizeof(f32) == 4, "Expected f32 to be 4 bytes.");
static_assert(sizeof(f64) == 8, "Expected f64 to be 8 bytes.");
static_assert(sizeof(b8) == 1, "Expected b8 to be 1 byte.");
static_assert(sizeof(b32) == 4, "Expected b32 to be 4 bytes.");

typedef u8 byte;

#if __LITTLE_ENDIAN__
#define MT_LITTLE_ENDIAN 1
#elif __BIG_ENDIAN__
#define MT_LITTLE_ENDIAN 0
#else
#error "Unknown endianness."
#endif

template<typename T>
T swap_endian(T u)
{
	static_assert(CHAR_BIT == 8, "CHAR_BIT != 8");

	union
	{
		T u;
		unsigned char u8[sizeof(T)];
	} source, dest;

	source.u = u;

	for (std::size_t k = 0; k < sizeof(T); k++)
	{
		dest.u8[k] = source.u8[sizeof(T) - k - 1];
	}

	return dest.u;
}

template<typename T>
static inline T from_little_endian(T u)
{
#if MT_LITTLE_ENDIAN
	return u;
#else
	return swap_endian(u);
#endif
}

template<typename T>
static inline T to_little_endian(T u)
{
#if MT_LITTLE_ENDIAN
	return u;
#else
	return swap_endian(u);
#endif
}

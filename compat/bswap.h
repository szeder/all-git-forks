/*
 * Let's make sure we always have a sane definition for ntohl()/htonl().
 * Some libraries define those as a function call, just to perform byte
 * shifting, bringing significant overhead to what should be a simple
 * operation.
 */

/*
 * Default version that the compiler ought to optimize properly with
 * constant values.
 */
static inline uint32_t default_swab32(uint32_t val)
{
	return (((val & 0xff000000) >> 24) |
		((val & 0x00ff0000) >>  8) |
		((val & 0x0000ff00) <<  8) |
		((val & 0x000000ff) << 24));
}

#undef bswap32

#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))

#define bswap32 git_bswap32
static inline uint32_t git_bswap32(uint32_t x)
{
	uint32_t result;
	if (__builtin_constant_p(x))
		result = default_swab32(x);
	else
		__asm__("bswap %0" : "=r" (result) : "0" (x));
	return result;
}

#elif defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))

#include <stdlib.h>

#define bswap32(x) _byteswap_ulong(x)

#endif

#ifdef bswap32

#undef ntohl
#undef htonl
#define ntohl(x) bswap32(x)
#define htonl(x) bswap32(x)

#ifndef __BYTE_ORDER
#	if defined(BYTE_ORDER) && defined(LITTLE_ENDIAN) && defined(BIG_ENDIAN)
#		define __BYTE_ORDER BYTE_ORDER
#		define __LITTLE_ENDIAN LITTLE_ENDIAN
#		define __BIG_ENDIAN BIG_ENDIAN
#	else
#		error "Cannot determine endianness"
#	endif
#endif

#if __BYTE_ORDER == __BIG_ENDIAN
# define ntohll(n) (n)
# define htonll(n) (n)
#elif __BYTE_ORDER == __LITTLE_ENDIAN
#	if defined(__GNUC__) && defined(__GLIBC__)
#		include <byteswap.h>
#	else /* GNUC & GLIBC */
static inline uint64_t bswap_64(uint64_t val)
{
	return ((val & (uint64_t)0x00000000000000ffULL) << 56)
		| ((val & (uint64_t)0x000000000000ff00ULL) << 40)
		| ((val & (uint64_t)0x0000000000ff0000ULL) << 24)
		| ((val & (uint64_t)0x00000000ff000000ULL) <<  8)
		| ((val & (uint64_t)0x000000ff00000000ULL) >>  8)
		| ((val & (uint64_t)0x0000ff0000000000ULL) >> 24)
		| ((val & (uint64_t)0x00ff000000000000ULL) >> 40)
		| ((val & (uint64_t)0xff00000000000000ULL) >> 56);
}
#	endif /* GNUC & GLIBC */
#	define ntohll(n) bswap_64(n)
#	define htonll(n) bswap_64(n)
#else /* __BYTE_ORDER */
#	error "Can't define htonll or ntohll!"
#endif

#endif

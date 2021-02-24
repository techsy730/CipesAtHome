// Functions and Macros for common logic that doesn't really belong anywhere else.
#ifndef CIPES_BASE_H
#define CIPES_BASE_H


#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include "absl/base/port.h"
#include <assert.h>

#ifdef INCLUDE_STACK_TRACES
#ifdef __GLIBC__ // Sorely lacking, but hopefully should work good enough.
#include <execinfo.h>
#endif
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ < 201112L
#error recipesAtHome requires at least C11 mode to build
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Most notable one this bring in is
// _CIPES_IS_WINDOWS
#include "internal/base_essentials.h"

extern bool _abrt_from_assert /*= false*/;

// This brings in
// _CIPES_STATIC_ASSERT
// _assert_with_stacktrace
#include "internal/base_assert.h"

// For some ridiculous reason, even though it has been part of C11 for like a decade now,
// some versions of Visual Studio do not support the "%z" printf specifier (for printing size_t's)
// So gotta work around it if that happens.
// _MSV_VER == 1800 is Visual Studio 2013
#if _CIPES_IS_WINDOWS && (!defined(_MSC_VER) || _MSV_VER < 1800)
#define _zf "%I64d"
#define _zuf "%I64u"
#else
#define _zf "%z"
#define _zuf "%zu"
#endif

#ifdef INCLUDE_STACK_TRACES
#define STACK_TRACE_FRAMES 15
ABSL_ATTRIBUTE_NOINLINE void printStackTraceF(FILE* f);
#else
ABSL_ATTRIBUTE_ALWAYS_INLINE inline void printStackTraceF(FILE* f) {}
#endif

#if !NO_PRINT_ON_MALLOC_FAIL
ABSL_ATTRIBUTE_ALWAYS_INLINE
#endif
ABSL_ATTRIBUTE_COLD
inline void handleMallocFailure() {
#if !NO_PRINT_ON_MALLOC_FAIL
    fprintf(stderr, "Fatal error! Ran out of heap memory.\n");
    fprintf(stderr, "Press enter to quit.\n");
    printStackTraceF(stderr);
    ABSL_ATTRIBUTE_UNUSED char exitChar = getchar();
#endif
}

 /*-------------------------------------------------------------------
 * Function 	: checkMallocFailed
 * Inputs	: void* p
 *
 * This function tests whether malloc failed to allocate (usually due
 * to lack of heap space), checked by giving the just malloc'ed pointer, p,
 * to this function.
 * If it was NULL, then this function will print an error message and
 * exit the program with a failure status.
 -------------------------------------------------------------------*/
ABSL_ATTRIBUTE_ALWAYS_INLINE inline void checkMallocFailed(const void* const p) {
#if !NO_MALLOC_CHECK
	if (ABSL_PREDICT_FALSE(p == NULL)) {
	  handleMallocFailure();
		exit(1);
	}
#endif
}

// MAX is NOT assured to not repeat side effects; x and y MUST be side effect free.
#if __cplusplus >= 201103L // C++11 or greater
#define MAX(x,y) ({ \
	auto _x = (x); \
	auto _y = (y); \
	_x > _y ?	_x : _y;})
#elif SUPPORTS_AUTOTYPE && !defined(__CDT_PARSER__)
#define MAX(x,y) ({ \
	__auto_type _x = (x); \
	__auto_type _y = (y); \
	_x > _y ?	_x : _y;})
#elif SUPPORTS_TYPEOF
#define MAX(x,y) ({ \
	typeof((x)) _x = (x); \
	typeof((y)) _y = (y); \
	_x > _y ?	_x : _y;})
#else
#define MAX(x,y) (((x) > (y)) ? (x) : (y))
#endif

// MIN is NOT assured to not repeat side effects; x and y MUST be side effect free.
#if __cplusplus >= 201103L // C++11 or greater
#define MIN(x,y) ({ \
	auto _x = (x); \
	auto _y = (y); \
	_x < _y ?	_x : _y;})
#elif SUPPORTS_AUTOTYPE && !defined(__CDT_PARSER__)
#define MIN(x,y) ({ \
	__auto_type _x = (x); \
	__auto_type _y = (y); \
	_x < _y ?	_x : _y;})
#elif SUPPORTS_TYPEOF
#define MIN(x,y) ({ \
	typeof((x)) _x = (x); \
	typeof((y)) _y = (y); \
	_x < _y ?	_x : _y;})
#else
#define MIN(x,y) (((x) < (y)) ? (x) : (y))
#endif

#if !ENABLE_PREFETCHING
#define _PREFETCH_READ_NO_TEMPORAL_LOCALITY(addr) _REQUIRE_SEMICOLON
// Annoyingly, abseil doesn't have a ABSL_PREFETCH or similar.
// So try to do it ourselves
#elif ABSL_HAVE_BUILTIN(__builtin_prefetch) || (defined(__GNUC__) && (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 2)))
// GCC or Clang
#define _PREFETCH_READ_NO_TEMPORAL_LOCALITY(addr) __builtin_prefetch((addr), 0, 0)
#elif ((defined(__x86_64__) && __x86_64__ == 1) || (defined(_M_X64)) && (defined(_MSC_VER) || defined(__INTEL_COMPILER))
// Visual Studio or ICC
#include <xmmintrin.h>
#define _PREFETCH_READ_NO_TEMPORAL_LOCALITY(addr) _mm_prefetch((addr), PREFETCHNTA)
#else
#define _PREFETCH_READ_NO_TEMPORAL_LOCALITY(addr) _REQUIRE_SEMICOLON
#endif
#endif

#ifdef __cplusplus
} // extern "C"
#endif

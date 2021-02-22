// Functions and Macros for common logic that doesn't really belong anywhere else.
#ifndef CRIPES_BASE_H
#define CRIPES_BASE_H


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

#ifndef _STR
#define _STR(x) #x
#endif
#ifndef _XSTR
#define _XSTR(x) _STR(x)
#endif

#if defined(_MSC_FULL_VER) || defined(_MSC_VER) || defined(__MINGW32__)
#define _CIPES_IS_WINDOWS 1
#else
#define _CIPES_IS_WINDOWS 0
#endif

#if defined(__GNUC__) || defined(__clang__)
#define COMPILER_WARNING(x) _Pragma(_STR(GCC warning x))
#else
#define COMPILER_WARNING(x) _Pragma(_STR(message (x)))
#endif

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

// __cplusplus == 201103L means C++11
#ifdef __cplusplus
#if __cplusplus >= 201103L && !defined(__CDT_PARSER__)
#define _REQUIRE_SEMICOLON_TOP_LEVEL_OK static_assert(true, "Never should see this error")
#else
#define _REQUIRE_SEMICOLON_TOP_LEVEL_OK struct ABSL_ATTRIBUTE_UNUSED __DoNotUse
#endif
#elif (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L) && !defined(__CDT_PARSER__)
// __STDC_VERSION__ == 201112L means C11
#define _REQUIRE_SEMICOLON_TOP_LEVEL_OK _Static_assert(true, "Never should see this error")
#else
#define _REQUIRE_SEMICOLON_TOP_LEVEL_OK ABSL_ATTRIBUTE_UNUSED static int __doNotUse
#endif

// Will NOT work in top level, use _REQUIRE_SEMICOLON_TOP_LEVEL_OK for that
#define _REQUIRE_SEMICOLON do {} while(0)

extern bool _abrt_from_assert /*= false*/;


// _MSC_VER 1910 is Visual Studio 2017
#if defined(__GNUC__) || defined(__clang__) || (defined(_MSC_VER) && _MSC_VER >= 1910)
#define SUPPORTS_TYPEOF 1
#endif

// Eclipse does not understand the __autotype syntax.
// The __CDT_PARSER__ macro is only active for Eclipse's parsing for symbols, not it's compilation.
#if !defined(__CDT_PARSER__) && (ABSL_HAVE_BUILTIN(__autotype) || (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 9))))
#define SUPPORTS_AUTOTYPE 1
#endif

// Eclipse does not like static_assert in C code even though it is part of the standard now.
// The __CDT_PARSER__ macro is only active for Eclipse's parsing for symbols, not it's compilation.
#ifdef __CDT_PARSER__
#define _CIPES_STATIC_ASSERT(condition, message) _REQUIRE_SEMICOLON_TOP_LEVEL_OK
// __cplusplus == 201103L means C++11
// __STDC_VERSION__ == 201112L means C11
#elif (defined(__cplusplus) && __cplusplus >= 201103L) \
	|| (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L)
#define _CIPES_STATIC_ASSERT(condition, message) static_assert(condition, message ": " #condition " (" _XSTR(condition) ") is NOT true")
#else
COMPILER_WARNING("Unable to use static_assert, static_asserts will be ignored")
#define _CIPES_STATIC_ASSERT(condition, message) _REQUIRE_SEMICOLON_TOP_LEVEL_OK
#endif

_CIPES_STATIC_ASSERT(true == 1, "true from stdbool.h must be 1 for the math to work correctly");


#ifdef NDEBUG
#define _assert_with_stacktrace(condition) _REQUIRE_SEMICOLON
#elif NO_STACK_TRACE_ASSERT
#define _assert_with_stacktrace(condition) assert((condition))
#elif defined(INCLUDE_STACK_TRACES)
#if __cplusplus >= 201103L // C++11 or greater
#define _assert_with_stacktrace(condition) ({ \
  auto _condition = ABSL_PREDICT_TRUE((condition)_; \
  if (!_condition) { \
  	_abrt_from_assert = true; \
  	printStackTraceF(stderr); \
  	fprintf(stderr, "%s (%s) is NOT true.\n", #condition, _XSTR(condition)); \
  	assert((condition)); \
	} \
})
#elif SUPPORTS_AUTOTYPE && !defined(__CDT_PARSER__)
#define _assert_with_stacktrace(condition) ({ \
  __auto_type _condition = ABSL_PREDICT_TRUE((condition)); \
  if (!_condition) { \
  	_abrt_from_assert = true; \
  	printStackTraceF(stderr); \
		fprintf(stderr, "%s (%s) is NOT true.\n", #condition, _XSTR(condition)); \
  	assert((condition)); \
  } \
})
#elif SUPPORTS_TYPEOF
#define _assert_with_stacktrace(condition) ({ \
  typeof(condition) _condition = ABSL_PREDICT_TRUE((condition)); \
  if (!_condition) { \
  	_abrt_from_assert = true; \
  	printStackTraceF(stderr); \
		fprintf(stderr, "%s (%s) is NOT true.\n", #condition, _XSTR(condition)); \
  	assert((condition)); \
	} \
})
#else
#define _assert_with_stacktrace(condition) ({ \
  if (! ABSL_PREDICT_TRUE((condition))) { \
  	_abrt_from_assert = true; \
  	printStackTraceF(stderr); \
		fprintf(stderr, "%s (%s) is NOT true.\n", #condition, _XSTR(condition)); \
  	assert((condition)); \
	} \
})
#endif
#else
#define _assert_with_stacktrace(condition) assert((condition))
#endif

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

// _MSC_VER == 1400 is Visual Studio 2005
#if (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L) || (defined(__GNUC__) && __GNUC__ > 3) || defined(__clang__) || (defined(_MSC_VER) && _MSC_VER >= 1400)
#define VA_MACROS_SUPPORTED 1
#else
#define VA_MACROS_SUPPORTED 0
#endif


#if VA_MACROS_SUPPORTED
// Taken from https://stackoverflow.com/questions/48045470/portably-detect-va-opt-support
#define _PP_THIRD_ARG(a,b,c,...) c
#define _VA_OPT_SUPPORTED_I(...) _PP_THIRD_ARG(__VA_OPT__(,),true,false,)
#define VA_OPT_SUPPORTED _VA_OPT_SUPPORTED_I(?)
#else
#define VA_OPT_SUPPORTED 0
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

// Functions and Macros for common logic that doesn't really belong anywhere else.
#ifndef CRIPES_BASE_H
#define CRIPES_BASE_H


#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include "absl/base/port.h"

#ifdef INCLUDE_STACK_TRACES
#ifdef __GLIBC__ // Sorely lacking, but hopefully should work good enough.
#include <execinfo.h>
#endif
#endif

#define STACK_TRACE_FRAMES 15

inline void printStackTraceF(FILE* f) {
#ifdef INCLUDE_STACK_TRACES
#ifdef __GLIBC__ // Sorely lacking, but hopefully should work good enough.
  void *array[STACK_TRACE_FRAMES];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, STACK_TRACE_FRAMES);

  char** backtrace_output = backtrace_symbols(array, size);
  if (backtrace_output == NULL) {
  	fprintf(f, "Unable to gather data for stack trace.\n");
  	return;
	}
  fprintf(f, "%s\n", *backtrace_output);
  free(backtrace_output);
#else
  fprintf(f, "Unable to gather data for stack trace (unsupported platform).\n");
#endif
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
inline void checkMallocFailed(const void* const p) {
	if (ABSL_PREDICT_FALSE(p == NULL)) {
		fprintf(stderr, "Fatal error! Ran out of heap memory.\n");
		fprintf(stderr, "Press enter to quit.\n");
		printStackTraceF(stderr);
		ABSL_ATTRIBUTE_UNUSED char exitChar = getchar();
		exit(1);
	}
}

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

// Annoyingly, abseil doesn't have a ABSL_PREFETCH or similar.
// So try to do it ourselves
#if ABSL_HAVE_BUILTIN(__builtin_prefetch) || (defined(__GNUC__) && (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 2)))
// GCC or Clang
#define _PREFETCH_READ_NO_TEMPORAL_LOCALITY(addr) __builtin_prefetch((addr), 0, 0)
#elif ((defined(__x86_64__) && __x86_64__ == 1) || (defined(_M_X64)) && (defined(_MSC_VER) || defined(__INTEL_COMPILER))
// Visual Studio or ICC
#include <xmmintrin.h>
#define _PREFETCH_READ_NO_TEMPORAL_LOCALITY(addr) _mm_prefetch((addr), PREFETCHNTA)
#else
// Handy hack to still require a semicolon
#define _PREFETCH_READ_NO_TEMPORAL_LOCALITY(addr) do { } while (0)
#endif

#endif

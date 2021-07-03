#ifndef STACKTRACE_H_
#define STACKTRACE_H_

#include <stdio.h>
#include "absl/base/port.h"
#include "internal/base_essentials.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef INCLUDE_STACK_TRACES
#define STACK_TRACE_FRAMES 40
#define WIN_MAX_STACK_TRACE_FRAMES 63
_CIPES_STATIC_ASSERT(STACK_TRACE_FRAMES < WIN_MAX_STACK_TRACE_FRAMES, "Windows can only fetch up to " _XSTR(WIN_MAX_STACK_TRACE_FRAMES) " frames at one, so keep this below that.");
ABSL_ATTRIBUTE_NOINLINE void printStackTraceF(FILE* f);
#else
ABSL_ATTRIBUTE_ALWAYS_INLINE inline void printStackTraceF(FILE* f) {}
#endif

#if defined(INCLUDE_STACK_TRACES) && _CIPES_IS_WINDOWS
// Prepare to initialize anything that needs to be done to initialize stacktracing.
// Ideally this should only be called once per program, but will have logic to prevent multiple initializing from causing problems.
// Also, if you fail to call this method, then printStackTraceF will call it for you, but is much more likely to fail at that point
// as by then you are typically in an error condition.
void prepareStackTraces();
#else
ABSL_ATTRIBUTE_ALWAYS_INLINE inline void prepareStackTraces() {}
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* STACKTRACE_H_ */

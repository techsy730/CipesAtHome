#include "base.h"

// Provides external definition (function body in header)
extern inline void checkMallocFailed(const void* const p);
bool _abrt_from_assert = false;

#ifdef INCLUDE_STACK_TRACES
ABSL_ATTRIBUTE_NOINLINE void printStackTraceF(FILE* f) {
#pragma omp critical(stack_trace)
	{
#ifdef __GLIBC__ // Sorely lacking, but hopefully should work good enough.
		void *array[STACK_TRACE_FRAMES];
		size_t size;

		// get void*'s for all entries on the stack
		size = backtrace(array, STACK_TRACE_FRAMES);
		char** backtrace_output = backtrace_symbols(array, size);
		if (backtrace_output == NULL || size == 0) {
			fprintf(f, "Unable to gather data for stack trace.\n");
			if (backtrace_output != NULL) {
				free(backtrace_output);
			}
			goto fail;				
		}
		// First one is likely going to be ourselves; reduce that importance.
		fprintf(f, "      (%s)\n", backtrace_output[0]);
		if (size > 1) {
			fprintf(f, "%s\n", backtrace_output[1]);
			for (size_t i = 2; i < size; ++i) {
				fprintf(f, "  %s\n", backtrace_output[i]);
			}
		}
		free(backtrace_output);
#else
		fprintf(f, "Unable to gather data for stack trace (unsupported platform).\n");
#endif
	fail: ;
	}
	return;
}
#else
ABSL_FORCE_ALWAYS_INLINE extern inline void printStackTraceF(FILE* f);
#endif


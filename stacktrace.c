#include "stacktrace.h"

#include <stdio.h>
#include <omp.h>

#ifdef __GLIBC__
#include <stdlib.h>  // For free()
#endif

#include "absl/base/port.h"
#include "openmp_once.h"


#if INCLUDE_STACK_TRACES
#ifdef __GLIBC__ // Sorely lacking, but hopefully should work good enough.
#include <execinfo.h>
#include <pthread.h>
#elif _CIPES_IS_WINDOWS
#include <string.h>
#include <windows.h>
#include <winbase.h>
// You cannot call these functions directly (not executing references like sizeof or typeof are fine).
// This has to be dynamically loaded.
// Type references, constants, and macros are fine though.
#include "dbghelp.h"
#endif // __GLIBC__, _CIPES_IS_WINDOWS
#endif // INCLUDE_STACK_TRACES

#if INCLUDE_STACK_TRACES
#if _CIPES_IS_WINDOWS

static bool getAndAddCurrentProcessId();
static bool tryLoadDbgHelpDll();
static bool tryLoadSymbolsWin();

#if SUPPORTS_TYPEOF
typedef typeof(SymSetOptions) *SymSetOptions_t;
typedef typeof(SymInitialize) *SymInitialize_t;
typedef typeof(SymFromAddr) *SymFromAddr_t;
typedef typeof(SymGetLineFromAddr) *SymGetLineFromAddr_t;
#else
typedef DWORD (IMAGEAPI *SymSetOptions_t)(DWORD SymOptions);
typedef WINBOOL (IMAGEAPI *SymInitialize_t)(HANDLE hProcess,PCSTR UserSearchPath,WINBOOL fInvadeProcess);
typedef WINBOOL (IMAGEAPI *SymFromAddr_t)(HANDLE hProcess,DWORD64 Address,PDWORD64 Displacement,PSYMBOL_INFO Symbol);
typedef WINBOOL (IMAGEAPI *SymGetLineFromAddr_t)(HANDLE hProcess,DWORD64 qwAddr,PDWORD pdwDisplacement,PIMAGEHLP_LINE Line);
#endif

static SymSetOptions_t SymSetOptions_func;
static SymInitialize_t SymInitialize_func;
static SymFromAddr_t SymFromAddr_func;
static SymGetLineFromAddr_t SymGetLineFromAddr_func;

#define STATIC_FRAME_STR_SIZE 200
static char frame_str_shared[STATIC_FRAME_STR_SIZE] = {0};
#pragma omp threadprivate(frame_str_shared)

// Depending on platform, we may have to load symbols once per thread.
static DWORD* known_process_ids = NULL;
static int known_process_id_count = 0;
static int known_process_id_capacity = 0;

static openmp_once_t load_dbghelp_dll = OPENMP_ONCE_INIT;
static openmp_once_t load_symbols = OPENMP_ONCE_INIT;
static openmp_once_t known_processes_ids_init = OPENMP_ONCE_INIT;
#pragma omp threadprivate(load_symbols, known_processes_ids_init)

static HANDLE first_thread_handle;
static DWORD first_thread_process_id = 0;
static openmp_once_t first_thread_id_init = OPENMP_ONCE_INIT;

static openmp_once_t first_thread_symbols_init = OPENMP_ONCE_INIT;

static HANDLE current_process_handle;
static DWORD current_process_id = 0;
#pragma omp threadprivate(current_process_handle, current_process_id)

void prepareStackTraces() {
	tryLoadDbgHelpDll();
	tryLoadSymbolsWin();
}
#else // !_CIPES_IS_WINDOWS
void prepareStackTraces();
#endif

ABSL_ATTRIBUTE_NOINLINE void printStackTraceF(FILE* f) {
#if _CIPES_IS_WINDOWS
	bool loaded_symbols = tryLoadSymbolsWin();
#endif
#pragma omp critical(stack_trace)
	{
#ifdef __GLIBC__ // Sorely lacking, but hopefully should work good enough.
		void *array[STACK_TRACE_FRAMES];
		size_t size;

		// get void*'s for all entries on the stack
		size = backtrace(array, STACK_TRACE_FRAMES);
		char** backtrace_output = backtrace_symbols(array, size);
		if (backtrace_output == NULL || size == 0) {
			fprintf(f, "Unable to gather data for stack trace (failed to allocate or got 0 frames)\n");
			if (backtrace_output != NULL) {
				free(backtrace_output);
			}
			goto fail;
		}
		// First one is likely going to be ourselves; reduce that importance.
		fprintf(f, "      0: (%s)\n", backtrace_output[0]);
		if (size > 1) {
			fprintf(f, "1: %s\n", backtrace_output[1]);
			for (size_t i = 2; i < size; ++i) {
				fprintf(f, "  %zu: %s\n", i, backtrace_output[i]);
			}
		}
		free(backtrace_output);
#elif _CIPES_IS_WINDOWS  // !__GLIBC__
		// Inspired by https://stackoverflow.com/a/31677858
#define STACK_TRACE_SKIP_FRAMES 0
		const WORD actual_frames_to_get = STACK_TRACE_FRAMES - STACK_TRACE_SKIP_FRAMES;
		void *stack[actual_frames_to_get];
#define SYMBOL_MAX_NAME_LEN 1024
		SYMBOL_INFO *info = NULL;
		if (loaded_symbols) {
			// max name len - 1 to account for the built-in base size of Name[1] array
			info = calloc(sizeof(SYMBOL_INFO) + (SYMBOL_MAX_NAME_LEN - 1)*(sizeof(char)), 1);
			info->MaxNameLen = SYMBOL_MAX_NAME_LEN;
		  info->SizeOfStruct = sizeof(SYMBOL_INFO);
		}
		IMAGEHLP_LINE line = {0};
		line.SizeOfStruct = sizeof(IMAGEHLP_LINE);
		DWORD64 symbol_displacement;
		DWORD line_displacement;

		WORD frames_gotten = CaptureStackBackTrace(STACK_TRACE_SKIP_FRAMES, actual_frames_to_get, stack, NULL);
		if (ABSL_PREDICT_TRUE(frames_gotten > 0)) {
			for (size_t i = 0; i < frames_gotten; ++i) {
				char* frame_str = frame_str_shared;
				bool frame_str_need_free = false;
				DWORD64 stack_addr = (DWORD64)stack[i];
				if (loaded_symbols) {
					bool got_sym = true;
					bool got_line = true;
					if (!SymFromAddr_func(current_process_handle, stack_addr, &symbol_displacement, info)) {
#ifdef STACK_TRACE_DEBUG
						printf("       Failed to get symbol of call address '0x" _zxf "' (Windows error code: %lu)\n", stack_addr, GetLastError());
#endif
						got_sym = false;
					}
					if (!SymGetLineFromAddr_func(current_process_handle, stack_addr, &line_displacement, &line)) {
#ifdef STACK_TRACE_DEBUG
						printf("       Failed to get line and of file of call address '0x" _zxf "' (Windows error code: %lu)\n", stack_addr, GetLastError());
#endif
						got_line = false;
					}
// #define ADDR_INFO_STR_LEN 50
//					char addr_info_str[ADDR_INFO_STR_LEN] = {0};
//					if (got_sym) {
//						sprintf(addr_info_str, "0x" _zfx "+%lu", stack_addr, symbol_displacement)
//					}
						// Harmless races; all threads will initialize these to the same values.
						static const char no_symbol[] = "(No symbol)";
						static const char no_line[] = "(No line information)";
#define LINE_INFO_STR_SHARED_LEN 1024
						static char line_info_str_shared[LINE_INFO_STR_SHARED_LEN];
#pragma omp threadprivate(line_info_str_shared)
						strncpy_s(line_info_str_shared, LINE_INFO_STR_SHARED_LEN, no_line, strlen(no_line));
						char *line_info_str = line_info_str_shared;
						bool line_info_str_need_free = false;
						if (got_line) {
							size_t chars_needed = snprintf(line_info_str, LINE_INFO_STR_SHARED_LEN, "%s:%I32u:%I32u", line.FileName, line.LineNumber, line_displacement);
							if (chars_needed > LINE_INFO_STR_SHARED_LEN) {
								char* line_info_str_new = malloc(chars_needed * sizeof(char));
								if (ABSL_PREDICT_FALSE(line_info_str_new == NULL)) {
									printf("Failed to allocate line info string. Information will be cutoff.\n");
								} else {
									line_info_str = line_info_str_new;
									line_info_str_need_free = true;
									snprintf(line_info_str, chars_needed, "%s:%I32u:%I32u", line.FileName, line.LineNumber, line_displacement);
								}
							}
						}
#ifdef STACK_TRACE_DEBUG
						if (got_sym) {
							// For debugging
							printf("'%s', %I32u, " _zuf "\n", info->Name, info->NameLen, symbol_displacement);
						}
#endif
#define MAX_SYMBOL_CHARS 200
#define FRAME_FORMAT_STR "0x" _zxf " (%.*s+" _zxf " : %s)"
						size_t chars_needed = snprintf(frame_str, STATIC_FRAME_STR_SIZE, FRAME_FORMAT_STR,
								stack_addr,
								(got_sym ? (int)MIN(info->NameLen, MAX_SYMBOL_CHARS) : (int)strlen(no_symbol)),
								(got_sym ? info->Name : no_symbol),
								(got_sym ? symbol_displacement : 0),
								line_info_str);
						if (chars_needed > STATIC_FRAME_STR_SIZE) {
							char* frame_str_new = malloc(chars_needed * sizeof(char));
							if (ABSL_PREDICT_FALSE(frame_str_new == NULL)) {
								printf("Failed to allocate frame info string. Information will be cutoff.\n");
							} else {
								frame_str = frame_str_new;
								frame_str_need_free = true;
								snprintf(frame_str, chars_needed, FRAME_FORMAT_STR,
																stack_addr,
																(got_sym ? (int)MIN(info->NameLen, MAX_SYMBOL_CHARS) : (int)strlen(no_symbol)),
																(got_sym ? info->Name : no_symbol),
																(got_sym ? symbol_displacement : 0),
																line_info_str);
							}
						}
						printf("%s" _zuf " : %s\n", (i == 0 ? "" : "  "), i + STACK_TRACE_SKIP_FRAMES, frame_str);
						if (line_info_str_need_free) {
							free(line_info_str);
						}
						if (frame_str_need_free) {
							free(frame_str);
						}
					}
				}
			}
// #endif // _CIPES_IS_WINDOWS
#else  // !_CIPES_IS_WINDOWS
		fprintf(f, "Unable to gather data for stack trace (unsupported platform).\n");
#endif
	fail: ;
	} // end omp critical(stack_trace)
	return;
}

#if _CIPES_IS_WINDOWS
// We can't link against dbghelp directly as that isn't distributed in MinGW.
// So we gotta load dynamically

#define NOT_ABLE_STACK_TRACES_FSTR "will not be able to print stack traces (Windows error code: %lu)"

// Inspired by https://stackoverflow.com/a/31677858

static FARPROC tryLoadFunction(HMODULE lib, const char* libName, const char* funcName) {
	FARPROC result = GetProcAddress(lib, funcName);
	if (ABSL_PREDICT_FALSE(result == NULL)) {
		printf("Failed to load function '%s' from %s; " NOT_ABLE_STACK_TRACES_FSTR "\n", funcName, libName, GetLastError());
	}
	return result;
}

static bool tryInsertProcessId(DWORD processId) {
	bool was_added = false;
#pragma omp critical(insert_process_id)
	{
		int omp_threads = omp_get_max_threads();
		if (omp_threads < 0) {
			omp_threads = 0;
		}
		// One for the main thread.
		int must_have_threads = omp_threads + 1;
		if (must_have_threads > known_process_id_capacity) {
			if (known_process_ids == NULL) {
				known_process_ids = calloc(must_have_threads, sizeof(int));
				checkMallocFailed(known_process_ids);
			} else {
				known_process_ids = realloc(known_process_ids, must_have_threads * sizeof(int));
				checkMallocFailed(known_process_ids);
			}
			known_process_id_capacity = must_have_threads;
		}
		size_t i = 0;
		bool found_me = false;
		// Linear search, yuck. But unless you are running a Threadripper or something, this should barely matter (plus only one time for each thread)
		for (; i < known_process_id_count; ++i) {
			if (known_process_ids[i] == current_process_id) {
				found_me = true;
			}
		}
		if (!found_me) {
			known_process_ids[i] = current_process_id;
			++known_process_id_count;
			was_added = true;
		}
	} // end omp critical(insert_process_id)
	return was_added;
}

static bool getAndAddCurrentProcessId() {
	if (!omp_in_parallel()) {
		static bool _main_thread_init_local = false;
		OPENMP_ONCE_WITH_LOCAL_NAME(_main_thread_init_local, first_thread_id_init, first_thread_init_process) {
#pragma omp critical(init_process_id_arr)
			{
				first_thread_handle = GetCurrentProcess();
				first_thread_process_id = GetCurrentProcessId();
			} // end critical(init_process_id_arr)
			first_thread_id_init.succeeded = tryInsertProcessId(first_thread_process_id);
		} OPENMP_ONCE_CLOSE
		return first_thread_id_init.succeeded;
	}
	OPENMP_ONCE(known_processes_ids_init, init_process_id_arr) {
		current_process_id = GetCurrentProcessId();
		if (first_thread_id_init.tried && current_process_id == first_thread_process_id) {
			current_process_handle = first_thread_handle;
			known_processes_ids_init.succeeded = false;
		} else {
			current_process_handle = GetCurrentProcess();
			OPENMP_ONCE(first_thread_id_init, first_thread_init_process) {
				first_thread_handle = current_process_handle;
				first_thread_process_id = current_process_id;
			} OPENMP_ONCE_CLOSE
		}
		known_processes_ids_init.succeeded = tryInsertProcessId(current_process_id);
	} OPENMP_ONCE_CLOSE
	return known_processes_ids_init.succeeded;
}

// GuardedBy critical(win_load_dll) Writes only
static SymInitialize_t SymInitialize_func = NULL;
// GuardedBy critical(win_load_dll) Writes only
static SymFromAddr_t SymFromAddr_func = NULL;

static bool tryLoadDbgHelpDll() {
	OPENMP_ONCE(load_dbghelp_dll, win_load_dll) {
		HMODULE dbgLib = LoadLibraryA("dbghelp.dll");
		if (ABSL_PREDICT_FALSE(dbgLib == NULL)) {
			printf("Unable to load dbghelp.dll; will not be able to include function names in stack traces (Windows error code: %lu)\n", GetLastError());
			load_dbghelp_dll.succeeded = false;
		} else {
			bool all_loaded = true;
			all_loaded |= !!(SymSetOptions_func = (SymSetOptions_t)tryLoadFunction(dbgLib, "dbghelp.dll", "SymSetOptions"));
			all_loaded |= !!(SymInitialize_func = (SymInitialize_t)tryLoadFunction(dbgLib, "dbghelp.dll", "SymInitialize"));
			all_loaded |= !!(SymFromAddr_func = (SymFromAddr_t)tryLoadFunction(dbgLib, "dbghelp.dll", "SymFromAddr"));
			all_loaded |= !!(SymGetLineFromAddr_func = (SymGetLineFromAddr_t)tryLoadFunction(dbgLib, "dbghelp.dll", "SymGetLineFromAddr"));
			if (all_loaded) {
				load_dbghelp_dll.succeeded = true;
			}
		}
	} OPENMP_ONCE_CLOSE
	return load_dbghelp_dll.succeeded;
}

static bool tryLoadSymbolsDo() {
	bool result = false;
#pragma omp critical(try_load_symbols_do)
	{
		SymSetOptions_func(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
		result = SymInitialize_func(current_process_handle, NULL, TRUE);
	} // end critical(try_load_symbols_do)
	return result;
}

static bool tryLoadSymbolsWin() {
	if (!load_dbghelp_dll.tried) {
		tryLoadDbgHelpDll();
	}
	if (!omp_in_parallel()) {
		if (getAndAddCurrentProcessId()) {
			static bool _main_thread_symbols_init_local = false;
			OPENMP_ONCE_WITH_LOCAL_NAME(_main_thread_symbols_init_local, first_thread_symbols_init, first_thread_load_symbols) {
				first_thread_symbols_init.succeeded = tryLoadSymbolsDo();
			} OPENMP_ONCE_CLOSE
		}
		return first_thread_symbols_init.succeeded;
	}
	if (!getAndAddCurrentProcessId()) {
		if (load_symbols.tried) {
			return load_symbols.succeeded;
		} else {
			bool first_thread_loaded_symbols = false;
#pragma omp critical(first_thread_load_symbols)
			{
				first_thread_loaded_symbols = first_thread_symbols_init.succeeded;
			} // end critical(first_thread_load_symbols)
			return first_thread_loaded_symbols;
		}
	}
	if (load_dbghelp_dll.succeeded) {
		OPENMP_ONCE(load_symbols, win_load_symbols) {
			load_symbols.succeeded = tryLoadSymbolsDo();
			OPENMP_ONCE(first_thread_symbols_init, first_thread_load_symbols) {
				first_thread_symbols_init.succeeded = load_symbols.succeeded;
			} OPENMP_ONCE_CLOSE
		} OPENMP_ONCE_CLOSE
		return load_symbols.succeeded;
	} else {
		return false;
	}
}
#endif // _CIPES_IS_WINDOWS

#else // !INCLUDE_STACK_TRACES
ABSL_ATTRIBUTE_ALWAYS_INLINE ABSL_ATTRIBUTE_UNUSED void prepareStackTraces();
ABSL_ATTRIBUTE_ALWAYS_INLINE ABSL_ATTRIBUTE_UNUSED void printStackTraceF(FILE* f);
#endif

/*
 * Macros for platform independent assertions.
 *
 * Do not include this header directly; it is meant to be included from base.h.
 */

#ifndef CIPES_BASE_ASSERT_H
#define CIPES_BASE_ASSERT_H
#ifndef CIPES_BASE_H
#error This header file is only supposed to be included from base.h
#endif

// Eclipse does not like static_assert in C code even though it is part of the standard now.
// The __CDT_PARSER__ macro is only active for Eclipse's parsing for symbols, not its compilation.
#ifdef __CDT_PARSER__
#define _CIPES_STATIC_ASSERT(condition, message) _REQUIRE_SEMICOLON_TOP_LEVEL_WITH_CUSTOM_STUB_NAME(doNotUse_stubbedOutStaticAssert)
// __cplusplus == 201103L means C++11
// __STDC_VERSION__ == 201112L means C11
#elif (defined(__cplusplus) && __cplusplus >= 201103L) \
	|| (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L)
#define _CIPES_STATIC_ASSERT(condition, message) static_assert(condition, message ": " #condition " (" _XSTR(condition) ") is NOT true")
#else
COMPILER_WARNING("Unable to use static_assert, static_asserts will be ignored")
#define _CIPES_STATIC_ASSERT(condition, message) _REQUIRE_SEMICOLON_TOP_LEVEL_WITH_CUSTOM_STUB_NAME(doNotUse_stubbedOutStaticAssert)
#endif

#ifdef NDEBUG
#define _assert_with_stacktrace(condition) ABSL_INTERNAL_ASSUME(condition)
#elif NO_STACK_TRACE_ASSERT
#define _assert_with_stacktrace(condition) assert(condition)
#elif defined(INCLUDE_STACK_TRACES)
#if __cplusplus >= 201103L // C++11 or greater
#define _assert_with_stacktrace(condition) ({ \
  auto _condition = ABSL_PREDICT_TRUE((condition)_; \
  if (!_condition) { \
  	_abrt_from_assert = true; \
    _Pragma("omp critical(printing_on_failure)") \
		{ \
			printStackTraceF(stderr); \
			fprintf(stderr, "%s (%s) is NOT true.\n", #condition, _XSTR(condition)); \
		} \
  	assert(condition); \
	} \
})
#elif SUPPORTS_AUTOTYPE && !defined(__CDT_PARSER__)
#define _assert_with_stacktrace(condition) ({ \
  __auto_type _condition = ABSL_PREDICT_TRUE(condition); \
  if (!_condition) { \
		_abrt_from_assert = true; \
  	_Pragma("omp critical(printing_on_failure)") \
		{ \
			printStackTraceF(stderr); \
			fprintf(stderr, "%s (%s) is NOT true.\n", #condition, _XSTR(condition)); \
		} \
		assert(condition); \
  } \
})
#elif SUPPORTS_TYPEOF
#define _assert_with_stacktrace(condition) ({ \
  typeof(condition) _condition = ABSL_PREDICT_TRUE(condition); \
  if (!_condition) { \
		_abrt_from_assert = true; \
  	_Pragma("omp critical(printing_on_failure)") \
		{ \
			printStackTraceF(stderr); \
			fprintf(stderr, "%s (%s) is NOT true.\n", #condition, _XSTR(condition)); \
		} \
		assert(condition); \
	} \
})
#else
#define _assert_with_stacktrace(condition) ({ \
  if (! ABSL_PREDICT_TRUE(condition)) { \
  	_abrt_from_assert = true; \
  	_Pragma("omp critical(printing_on_failure)") \
		{ \
			printStackTraceF(stderr); \
			fprintf(stderr, "%s (%s) is NOT true.\n", #condition, _XSTR(condition)); \
		} \
  	assert(condition); \
	} \
})
#endif
#else
#define _assert_with_stacktrace(condition) assert(condition)
#endif

#endif /* _CIPES_BASE_ASSERT_H */

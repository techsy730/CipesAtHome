/*
 * Macros for a lot of definitions of other macros.
 *
 * Do not include this header directly; it is meant to be included from base.h.
 */

#ifndef CIPES_BASE_ESSENTIALS_H
#define CIPES_BASE_ESSENTIALS_H

// Pre declare these for code completion

// Whether to (try to) print stack traces on crashing failures
#ifndef INCLUDE_STACK_TRACES
#define INCLUDE_STACK_TRACES 0
#endif

// Whether to put additional checks on the shifting left and right functions in calculator.c
#ifndef VERIFYING_SHIFTING_FUNCTIONS
#define VERIFYING_SHIFTING_FUNCTIONS 0
#endif

// Whether to be more aggressive 0-ing out new data structures (e.g. calloc vs malloc)
#ifndef AGGRESSIVE_0_ALLOCATING
#define AGGRESSIVE_0_ALLOCATING 0
#endif

#ifndef _STR
#define _STR(x) #x
#endif
#ifndef _XSTR
#define _XSTR(x) _STR(x)
#endif

#if defined(_M_X64) || defined(__amd64__) || defined(__ppc64__) || defined(__LP64__) || defined(_LP64) || defined(WIN64) || defined(_WIN64)
#define _CIPES_64_BIT 1
#else
#define _CIPES_32_BIT 1
#endif


#if defined(WIN32) || defined(WIN64) || defined(_WIN32) || defined(_WIN64) || defined(_MSC_FULL_VER) || defined(_MSC_VER) || defined(__MINGW32__)
#define _CIPES_IS_WINDOWS 1
#else
#define _CIPES_IS_WINDOWS 0
#endif

#if defined(__GNUC__) || defined(__clang__)
#define COMPILER_WARNING(x) _Pragma(_STR(GCC warning x))
#else
#define COMPILER_WARNING(x) _Pragma(_STR(message (x)))
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

// __cplusplus == 201103L means C++11
#ifdef __cplusplus
#if __cplusplus >= 201103L//  && !defined(__CDT_PARSER__)
#define _REQUIRE_SEMICOLON_TOP_LEVEL_WITH_CUSTOM_STUB_NAME(ignored) static_assert(true, "Never should see this error")
#define _REQUIRE_SEMICOLON_TOP_LEVEL_OK _REQUIRE_SEMICOLON_TOP_LEVEL_WITH_CUSTOM_STUB_NAME(DoNotUse)
#else
#define _REQUIRE_SEMICOLON_TOP_LEVEL_WITH_CUSTOM_STUB_NAME(n) struct ABSL_ATTRIBUTE_UNUSED __##n
#define _REQUIRE_SEMICOLON_TOP_LEVEL_OK _REQUIRE_SEMICOLON_TOP_LEVEL_WITH_CUSTOM_STUB_NAME(DoNotUse)
#endif
#elif (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L) && !defined(__CDT_PARSER__)
// __STDC_VERSION__ == 201112L means C11
#define _REQUIRE_SEMICOLON_TOP_LEVEL_WITH_CUSTOM_STUB_NAME(ignored) _Static_assert(true, "Never should see this error")
#define _REQUIRE_SEMICOLON_TOP_LEVEL_OK _REQUIRE_SEMICOLON_TOP_LEVEL_WITH_CUSTOM_STUB_NAME(doNotUse)
#else
#define _REQUIRE_SEMICOLON_TOP_LEVEL_WITH_CUSTOM_STUB_NAME(n) ABSL_ATTRIBUTE_UNUSED static int __##n
#define _REQUIRE_SEMICOLON_TOP_LEVEL_OK ABSL_ATTRIBUTE_UNUSED _REQUIRE_SEMICOLON_TOP_LEVEL_WITH_CUSTOM_STUB_NAME(doNotUse)
#endif

// Will NOT work in top level, use _REQUIRE_SEMICOLON_TOP_LEVEL_OK for that
#define _REQUIRE_SEMICOLON do {} while(0)


// _MSC_VER 1910 is Visual Studio 2017
#if defined(__GNUC__) || defined(__clang__) || (defined(_MSC_VER) && _MSC_VER >= 1910)
#define SUPPORTS_TYPEOF 1
#endif

// Eclipse does not understand the __autotype syntax.
// The __CDT_PARSER__ macro is only active for Eclipse's parsing for symbols, not it's compilation.
#if !defined(__CDT_PARSER__) && (ABSL_HAVE_BUILTIN(__autotype) || (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 9))))
#define SUPPORTS_AUTOTYPE 1
#endif

#endif /* CIPES_BASE_ESSENTIALS_H */

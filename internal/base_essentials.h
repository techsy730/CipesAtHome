/*
 * Macros for a lot of definitions of other macros.
 *
 * Do not include this header directly; it is meant to be included from base.h.
 */

#ifndef CIPES_BASE_ESSENTIALS_H
#define BASE_ESSENTIALS_H
#ifndef CIPES_BASE_H
#error This header file is only supposed to be included from base.h
#endif

#ifndef _STR
#define _STR(x) #x
#endif
#ifndef _XSTR
#define _XSTR(x) _STR(x)
#endif

#if defined(WIN32) || defined(WIN64) || defined(_MSC_FULL_VER) || defined(_MSC_VER) || defined(__MINGW32__)
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
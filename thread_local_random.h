/*
 * thread_local_random.h
 *
 *  Created on: Jun 26, 2021
 *      Author: sean
 */

#ifndef _THREAD_LOCAL_RANDOM_H_
#define _THREAD_LOCAL_RANDOM_H_

#include "absl/base/attributes.h"
#include "base.h"

#if _POSIX_C_SOURCE >= 1 || _XOPEN_SOURCE || _POSIX_SOURCE
#define _HAS_RANDR 1
#endif

#if _USE_LEHMER64_RANDOM
#include <stdint.h>
#define _NEED_EXTERN_DEF 1
typedef uint64_t random_output_t;
typedef uint64_t random_seed_t;
#elif _USE_CPP_RANDOM
#include "cpp_random_adapter.h"
typedef random_output_cpp_t random_output_t;
typedef random_seed_cpp_t random_seed_t;
#else
#define _USING_SYSTEM_RAND 1
#include <stdlib.h>
#if !_HAS_RANDR
#include "internal/real_rands.h"
#endif
typedef int random_output_t;
typedef unsigned random_seed_t;
#endif

ABSL_ATTRIBUTE_ALWAYS_INLINE
inline random_output_t _internal_scale_to_bounds(random_output_t raw, random_output_t low, random_output_t high) {
	// Not an ideal way, but we can tolerate a bit non-uniformity.
	_assert_with_stacktrace(low <= high);
	return (high - low != 0) ? (raw % (high - low)) + low : low;
}

#if _USING_SYSTEM_RAND && _HAS_RANDR
extern random_seed_t _threadlocal_seed;
#pragma omp threadprivate(_threadlocal_seed)
#endif

#if _NEED_EXTERN_DEF

void threadlocal_rand_init();
void threadlocal_srand(random_seed_t seed);
random_output_t threadlocal_rand();
random_output_t threadlocal_randint(random_output_t low, random_output_t high);
void threadlocal_rand_destroy();

#else

ABSL_ATTRIBUTE_ALWAYS_INLINE
inline void threadlocal_rand_init() {
#if _USE_CPP_RANDOM
	cpp_rand_init();
#endif
}

ABSL_ATTRIBUTE_ALWAYS_INLINE
inline void threadlocal_srand(random_seed_t seed) {
#if _USE_CPP_RANDOM
	cpp_srand(seed);
#elif _HAS_RANDR
	_threadlocal_seed = seed;
#else
	real_srand(seed);
#endif
}

ABSL_ATTRIBUTE_ALWAYS_INLINE
inline random_output_t threadlocal_rand() {
#if _USE_CPP_RANDOM
	return cpp_rand();
#elif _HAS_RANDR
	return rand_r(&_threadlocal_seed);
#else
	// Better a thread unsafe rand then nothing.
	// But on Windows this is threadsafe for free!
	return real_rand();
#endif
}

ABSL_ATTRIBUTE_ALWAYS_INLINE
inline random_output_t threadlocal_randint(random_output_t low, random_output_t high) {
#if _USE_CPP_RANDOM
	return cpp_randint(low, high);
#elif _USING_SYSTEM_RAND && _HAS_RANDR
	return _internal_scale_to_bounds(threadlocal_rand(), low, high);
#else
	return _internal_scale_to_bounds(real_rand(), low, high);
#endif
}

ABSL_ATTRIBUTE_ALWAYS_INLINE
inline void threadlocal_rand_destroy() {
#if _USE_CPP_RANDOM
	cpp_rand_destroy();
#endif
}

#endif // _NEED_EXTERN_DEF

#endif /* _THREAD_LOCAL_RANDOM_H_ */

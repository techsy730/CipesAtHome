/*
 * thread_local_random.c
 *
 *  Created on: Jun 26, 2021
 *      Author: sean
 */

#include "thread_local_random.h"
#if _NEED_EXTERN_DEF
#include "absl/base/attributes.h"
#endif

#if _USE_LEHMER64_RANDOM
#include "lehmer64_cipes.h"
#elif _USING_SYSTEM_RAND && _HAS_RANDR
unsigned int _threadlocal_seed;
#pragma omp threadprivate(_threadlocal_seed)
#endif

#if !_NEED_EXTERN_DEF

#if _USE_LEHMER64_RANDOM
#error Assertion error: External definitions should be needed but somehow they are not
#endif

ABSL_ATTRIBUTE_UNUSED
extern inline void threadlocal_rand_init();
ABSL_ATTRIBUTE_UNUSED
extern inline void threadlocal_srand(random_seed_t seed);
ABSL_ATTRIBUTE_UNUSED
extern inline random_output_t threadlocal_rand();
ABSL_ATTRIBUTE_UNUSED
extern inline random_output_t threadlocal_randint(random_output_t low, random_output_t high);
ABSL_ATTRIBUTE_UNUSED
extern inline void threadlocal_rand_destroy();

#else

ABSL_ATTRIBUTE_ALWAYS_INLINE
inline void threadlocal_rand_init() {}

void threadlocal_srand(random_seed_t seed) {
	lehmer64_seed(seed);
}

random_output_t threadlocal_rand() {
	return lehmer64();
}

random_output_t threadlocal_randint(random_output_t low, random_output_t high) {
	return _internal_scale_to_bounds(threadlocal_rand(), low, high);
}

ABSL_ATTRIBUTE_ALWAYS_INLINE
inline void threadlocal_rand_destroy() {}

#endif // _USE_LEHMER64_RANDOM

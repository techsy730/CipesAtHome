/*
 * cpp_random_adapter.h
 *
 *  Created on: Jun 14, 2021
 *      Author: TechSY730
 */

#ifndef CPP_RANDOM_ADAPTER_H_
#define CPP_RANDOM_ADAPTER_H_

#if _USE_CPP_RANDOM

#include <stdint.h>
#include "base.h"

#ifdef __cplusplus
namespace cpp_random_adapter {
extern "C" {
#endif

#if _USE_XOSHIRO_RANDOM
typedef uint64_t random_output_cpp_t;
typedef unit64_t random_seed_cpp_t;
#else
#if _CIPES_64_BIT && !_USE_LCG_RANDOM
// These will be checked for consistency statically in the cpp file
typedef uint_fast64_t random_output_cpp_t;
typedef uint_fast64_t random_seed_cpp_t;
#else
typedef uint_fast32_t random_output_cpp_t;
typedef uint_fast32_t random_seed_cpp_t;
#endif
#endif

void cpp_rand_init();

void cpp_srand(random_seed_cpp_t seed);

random_output_cpp_t cpp_rand();

random_output_cpp_t cpp_randint(random_output_cpp_t low, random_output_cpp_t high);

void cpp_rand_destroy();

#ifdef __cplusplus
} // extern "C"
} // namespace cpp_random_adapter
#endif

#endif // _USE_CPP_RANDOM

#endif /* CPP_RANDOM_ADAPTER_H_ */

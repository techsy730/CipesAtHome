/*
 * cpp_random_adapter_generator_selection.h
 *
 *  Created on: Jul 2, 2021
 *      Author: sean
 */

#ifndef INTERNAL_CPP_RANDOM_ADAPTER_GENERATOR_SELECTION_H_
#define INTERNAL_CPP_RANDOM_ADAPTER_GENERATOR_SELECTION_H_

#ifdef __cplusplus
#include <random>
namespace cpp_random_adapter {
namespace internal {
#ifdef _USE_CUSTOM_CPP_RANDOM
typedef _USE_CUSTOM_CPP_RANDOM RandomGenType;
#elif _USE_XOSHIRO_RANDOM
// We can't reference the exact types as the XoshiroCpp header must be included once and only once in any executable
#elif _USE_LCG_RANDOM
typedef std::minstd_rand RandomGenType;
#elif _USE_MERSENNE_TWISTER_RANDOM
#if _CIPES_64_BIT
typedef std::mt19937_64 RandomGenType;
#else
typedef std::mt19937 RandomGenType;
#endif
#elif _USE_SUBTRACT_WITH_CARRY_RANDOM
#if _CIPES_64_BIT
typedef std::ranlux48_base RandomGenType;
#else
typedef std::ranlux24_base RandomGenType;
#endif
#else
#error Asked for a C++ random generator, but no implementation selected.
#endif
} // namespace internal
} // namespace cpp_random_adapter
#endif // __cplusplus

#endif /* INTERNAL_CPP_RANDOM_ADAPTER_GENERATOR_SELECTION_H_ */

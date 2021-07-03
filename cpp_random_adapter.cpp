/*
 * Adapts the C++ random library into a form that C code can use.
 *
 * random_adapter.cpp
 *
 *  Created on: Jun 14, 2021
 *      Author: TechSY730
 */

#if defined(__CDT_PARSER__) && !defined(_USE_CPP_RANDOM)
// Just so this file can actually work in Eclipse C++ IDE, even when this flag isn't set
#define _USE_CPP_RANDOM 1
#endif
#if _USE_CPP_RANDOM

#include "cpp_random_adapter.h"
#include "internal/cpp_random_adapter_generator_selection.h"

#include "absl/base/policy_checks.h"
#include <random>
#include <optional>
#include <limits>
#include "base.h"
#include "absl/base/optimization.h"
#if _USE_XOSHIRO_RANDOM
#include "Xoshiro-cpp/XoshiroCpp.hpp"
#endif

namespace cpp_random_adapter {
#if _USE_XOSHIRO_RANDOM
namespace internal {
#if _CIPES_64_BIT
typedef XoshiroCpp::Xoshiro256StarStar RandomGenType;
#else
typedef XoshiroCpp::Xoshiro128StarStar RandomGenType;
#endif
} // namespace internal
#endif

using internal::RandomGenType;

static thread_local std::optional<RandomGenType> random_generator;
static thread_local std::uniform_int_distribution like_rand_distribution(0, RAND_MAX);
static thread_local std::uniform_int_distribution zero_to_99_distribution(0, 99);
static thread_local std::uniform_int_distribution one_to_hundred_distribution(1, 100);

static void init_if_needed() {
	if (ABSL_PREDICT_FALSE(!random_generator.has_value())) {
		random_generator.emplace();
	}
}

void cpp_rand_init() {
	init_if_needed();
}

void cpp_srand(random_seed_cpp_t seed) {
#if _USE_XOSHIRO_RANDOM
	random_generator.emplace(XoshiroCpp::SplitMix64(seed)());
#else
	random_generator.emplace(seed);
#endif
}

random_output_cpp_t cpp_rand() {
	init_if_needed();
	return like_rand_distribution(*random_generator);
}

random_output_cpp_t cpp_randint(random_output_cpp_t min, random_output_cpp_t max) {
	init_if_needed();
	if (min == 0 && max == RAND_MAX) {
		return like_rand_distribution(*random_generator);
	} else if (min == 1 && max == 100) {
		return one_to_hundred_distribution(*random_generator);
	} else if (min == 0 && max == 99) {
		return zero_to_99_distribution(*random_generator);
	} else {
		std::uniform_int_distribution dist(min, max);
		return dist(*random_generator);
	}
}

void cpp_rand_destroy() {
	random_generator.reset();
}

} // namespace cpp_random_adapter

#endif

/*
 * Adapts the C++ random library into a form that C code can use.
 *
 * random_adapter.cpp
 *
 *  Created on: Jun 14, 2021
 *      Author: TechSY730
 */

#include "absl/base/policy_checks.h"
#include <random>
#include <optional>
#include <limits>
#include "base.h"
#include "Xoshiro-cpp/XoshiroCpp.hpp"
#include "absl/base/optimization.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "random_adapter.h"

#if _CIPES_64_BIT
typedef XoshiroCpp::Xoshiro256StarStar RandomGenType;
static thread_local std::optional<RandomGenType> random_generator;
#else
typedef XoshiroCpp::Xoshiro128StarStar RandomGenType;
static thread_local std::optional<XoshiroCpp::RandomGenType> random_generator;
#endif

static thread_local std::uniform_int_distribution like_rand_distribution(0, RAND_MAX);
static thread_local std::uniform_int_distribution zero_to_99_distribution(0, 99);
static thread_local std::uniform_int_distribution one_to_hundred_distribution(1, 100);

static void init_if_needed() {
	if (ABSL_PREDICT_FALSE(!random_generator.has_value())) {
		random_generator.emplace();
	}
}

void threadlocal_srand(unsigned seed) {
	random_generator.emplace(XoshiroCpp::SplitMix64(seed)());
}

int threadlocal_rand() {
	init_if_needed();
	return like_rand_distribution(*random_generator);
}

int threadlocal_randint(int min, int max) {
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

void threadlocal_rand_destroy() {
	if (random_generator.has_value()) {
		random_generator.reset();
	}
}

#ifdef __cplusplus
}
#endif

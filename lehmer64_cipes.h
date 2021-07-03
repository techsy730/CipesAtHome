/*
 * lehmer64_cipes.h
 *
 * Thanks to declaring a static variable in a header, we can't really use lemire's header file directly in a reusable way.
 * So this file is a copy of lehmer64.h from https://github.com/lemire/testingRNG/blob/master/source/lehmer64.h
 * from revision bfd776ba13b837bc1680de08e5de389a7f44f10d (dated Mon Dec 28 15:23:30 2020 -0500).
 *
 * Note, because the original file had static variables, the only way to thread private them was to alter the source itself.
 * Also note the because this header has static members declared, one and only one non-exported (so not header) file should import this.
 *
 *  Created on: Jun 26, 2021
 *  		Author: lemire
 *      Modified by: TechSY730
 */

#ifndef LEHMER64_CIPES_H_
#define LEHMER64_CIPES_H_

#include "lemire-testingRNG/source/splitmix64.h"

static __uint128_t g_lehmer64_state;

#pragma omp threadprivate(g_lehmer64_state)

/**
* D. H. Lehmer, Mathematical methods in large-scale computing units.
* Proceedings of a Second Symposium on Large Scale Digital Calculating
* Machinery;
* Annals of the Computation Laboratory, Harvard Univ. 26 (1951), pp. 141-146.
*
* P L'Ecuyer,  Tables of linear congruential generators of different sizes and
* good lattice structure. Mathematics of Computation of the American
* Mathematical
* Society 68.225 (1999): 249-260.
*/

static inline void lehmer64_seed(uint64_t seed) {
  g_lehmer64_state = (((__uint128_t)splitmix64_stateless(seed)) << 64) +
                     splitmix64_stateless(seed + 1);
}

static inline uint64_t lehmer64() {
  g_lehmer64_state *= UINT64_C(0xda942042e4dd58b5);
  return g_lehmer64_state >> 64;
}

#endif


#ifndef INTERNAL_LEHMER64_H_
#define INTERNAL_LEHMER64_H_

#endif /* INTERNAL_LEHMER64_CIPES_H_ */

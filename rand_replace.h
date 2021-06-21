/*
 * rand_replace.h
 *
 * Switches "rand" implementations as needed.
 *
 *  Created on: Jun 20, 2021
 *      Author: TechSY730
 */

#ifndef RAND_REPLACE_H_
#define RAND_REPLACE_H_

#include "random_adapter.h"
#include "absl/base/port.h"

#define rand threadlocal_rand

ABSL_ATTRIBUTE_ALWAYS_INLINE
inline int randint(int min, int max) {
	return threadlocal_randint(min, max);
}

#define srand threadlocal_srand

#endif /* RAND_REPLACE_H_ */

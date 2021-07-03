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

#include "thread_local_random.h"
#include "internal/real_rands.h"

// Redefining built-ins is a risky proposition for sure, but seems to work fine on
// most compilers as few try to do any fancy "compiler magic" with rand due to the (by nature) non "pureness" of it.
#define rand threadlocal_rand

#define randint threadlocal_randint

#define srand threadlocal_srand

#define _RAND_REPLACE_REPLACEMENTS_DONE 1

#endif /* RAND_REPLACE_H_ */

/*
 * real_rands.h
 *
 *  Created on: Jun 26, 2021
 *      Author: sean
 */

#ifndef INTERNAL_REAL_RANDS_H_
#define INTERNAL_REAL_RANDS_H_

#ifdef _RAND_REPLACE_REPLACEMENTS_DONE
#error This header file must be included _before_ rand_replace.h is included
#endif

#include <stdlib.h>

typedef int rand_f_type();
typedef void srand_f_type(unsigned int);

rand_f_type* const real_rand = rand;
srand_f_type* const real_srand = srand;

#endif /* INTERNAL_REAL_RANDS_H_ */

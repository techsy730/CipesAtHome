/*
 * random_adapter.h
 *
 *  Created on: Jun 14, 2021
 *      Author: sean
 */

#ifndef RANDOM_ADAPTER_H_
#define RANDOM_ADAPTER_H_

#ifdef __cplusplus
extern "C" {
#endif

void threadlocal_rand_init();

void threadlocal_srand(unsigned seed);

int threadlocal_rand();

int threadlocal_randint(int low, int high);

void threadlocal_rand_destroy();

#ifdef __cplusplus
}
#endif

#endif /* RANDOM_ADAPTER_H_ */

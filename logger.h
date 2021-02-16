#ifndef LOGGER_H
#define LOGGER_H

#include <stdbool.h>
#include "absl/base/port.h"

extern int level_cfg;

int init_level_cfg();
ABSL_ATTRIBUTE_ALWAYS_INLINE inline int get_log_level() {
	return level_cfg;
}
ABSL_ATTRIBUTE_ALWAYS_INLINE inline bool will_log_level(int level) {
	return level <= level_cfg;
}
int recipeLog(int level, char *process, char *subProcess, char *activity, char *entry);
#endif


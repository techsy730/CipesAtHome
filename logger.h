#ifndef LOGGER_H
#define LOGGER_H

#include <stdbool.h>
#include "absl/base/port.h"

int init_level_cfg();
int get_log_level();
ABSL_ATTRIBUTE_ALWAYS_INLINE inline bool will_log_level(int level) {
	return level <= get_log_level();
}
int recipeLog(int level, char *process, char *subProcess, char *activity, char *entry);
#endif

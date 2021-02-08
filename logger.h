#ifndef LOGGER_H
#define LOGGER_H

#import <stdbool.h>
#import "absl/base/port.h"

int init_level_cfg();
inline int get_log_level();
inline bool can_be_logged(int level) {
	return 
int recipeLog(int level, char *process, char *subProcess, char *activity, char *entry);
#endif

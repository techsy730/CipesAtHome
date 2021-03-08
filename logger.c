#include "logger.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "base.h"
#include <time.h>

int level_cfg = 1;

int init_level_cfg() {
	level_cfg = getConfigInt("logLevel");
	return 0;
}

#define PREALLOCATED_SHARED_SIZE 200

static char shared_data[PREALLOCATED_SHARED_SIZE];
#pragma omp threadprivate(shared_data)

int recipeLog(int level, char *process, char *subProcess, char *activity, char *entry) {
	if (level_cfg >= level) {
		int hours, mins, secs, month, day, year;
		time_t now;
		time(&now);
		struct tm* local = localtime(&now);
		hours = local->tm_hour;
		mins = local->tm_min;
		secs = local->tm_sec;
		day = local->tm_mday;
		month = local->tm_mon + 1;
		year = local->tm_year + 1900;
		char date[100];
		char* data = shared_data;
		bool need_data_free = false;
		sprintf(date, "[%d-%02d-%02d %02d:%02d:%02d]", year, month, day, hours, mins, secs);
		size_t needed_size = snprintf(data, PREALLOCATED_SHARED_SIZE, "[%s][%s][%s][%s]\n", process, subProcess, activity, entry);
		if (ABSL_PREDICT_FALSE(needed_size > PREALLOCATED_SHARED_SIZE)) {
			data = malloc(sizeof(char)*needed_size);
			checkMallocFailed(data);
			snprintf(data, needed_size, "[%s][%s][%s][%s]\n", process, subProcess, activity, entry);
			need_data_free = true;
		}

		printf("%s", date);
		printf("%s", data);

		FILE* fp = fopen("recipes.log", "a");
		fputs(date, fp);
		fputs(data, fp);

		fclose(fp);

		if (need_data_free) {
			free(data);
			data = NULL;
		}
		// Effectively empty out now old data.
		shared_data[0] = 0;
	}
	return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <libconfig.h>
#include "inventory.h"
#include "config.h"
#include "recipes.h"
#include "FTPManagement.h"
#include "start.h"
#include "calculator.h"
#include <time.h>
#include "cJSON.h"
#include <curl/curl.h>
#include "logger.h"
#include <sys/stat.h>
#include <sys/types.h>

int current_frame_record;
const char *local_ver;

int getLocalRecord() {
	return current_frame_record;
}
void setLocalRecord(int frames) {
	current_frame_record = frames;
}

const char *getLocalVersion() {
	return local_ver;
}

int main(int argc, char **argv) {

	int max_outer_loops = -1;
	long max_branches = -1;
	if (argc >= 2) {
		max_branches = atol(argv[1]);
		if (argc >= 3) {
			max_outer_loops = atoi(argv[2]);
		}
	}

	current_frame_record = 9999;
	initConfig();

	// If select and randomise are both 0, the same roadmap will be calculated on every thread, so set threads = 1
	// The debug setting can only be meaningfully used with one thread as well.
	int workerCount = (getConfigInt("select") || getConfigInt("randomise"))
					  && !getConfigInt("debug") ? getConfigInt("workerCount") : 1;

	// Create workerCount threads
	omp_set_num_threads(workerCount);

	totalIterations = calloc(workerCount, sizeof(long));

	local_ver = getConfigStr("Version");
	init_level_cfg();
	curl_global_init(CURL_GLOBAL_DEFAULT);	// Initialize libcurl

#if 0  // Disabled for profiling
	int update = checkForUpdates(local_ver);

	// Greeting message to user
	printf("Welcome to Recipes@Home!\n");
	printf("Leave this program running as long as you want to search for new recipe orders.\n");
	int blob_record = getFastestRecordOnBlob();
	if (blob_record == 0) {
		printf("There was an error contacting the server to retrieve the fastest time.\n");
		printf("Please check your internet connection, but we'll continue for now.\n");
	}
	else {
		printf("The current fastest record is %d frames. Happy cooking!\n", blob_record);
	}

	if (update == -1) {
		printf("Could not check version on Github. Please check your internet connection.\n");
		printf("Otherwise, we can't submit completed roadmaps to the server!\n");
		printf("Alternatively you may have been rate-limited. Please wait a while and try again.\n");
		printf("Press ENTER to quit.\n");
		char exitChar = getchar();
		return -1;
	}
	else if (update == 1) {
		printf("Please visit https://github.com/SevenChords/CipesAtHome/releases to download the newest version of this program!\n");
		printf("Press ENTER to quit.\n");
		char exitChar = getchar();
		return -1;
	}
#endif

	// Verify that username field is not malformed,
	// as this would cause errors when a roadmap is submitted to the servers
	if (getConfigStr("Username") == NULL) {
		printf("Username field is malformed. Please verify that your username is within quotation marks next to \"Username = \"\n");
		printf("Press ENTER to exit the program.\n");
		char exitChar = getchar();
		exit(1);
	}

	// Verify that the results folder exists
	// If not, create the directory
	mkdir("./results", 0777);

	// To avoid generating roadmaps that are slower than the user's record best,
	// use PB.txt to identify the user's current best
	FILE* fp = fopen("results/PB.txt", "r");

	// The PB file may not have been created yet, so ignore the case where it is missing
	if (fp != NULL) {
		int PB_record;
		if (fscanf(fp, "%d", &PB_record) == 1) {
			current_frame_record = PB_record;
			testRecord(current_frame_record);
		}
		fclose(fp);

		// Submit the user's fastest roadmap to the server for leaderboard purposes
	}

	// Initialize global variables in calculator.c
	// This does not need to be done in parallel, as these globals will
	// persist through all parallel calls to calculator.c
	initializeInvFrames();
	initializeRecipeList();

	// copying to const so OpenMP knows it doesn't have to have each thread
	// recheck this.
	const int max_outer_loops_fixed = max_outer_loops;
	const long max_branches_fixed = max_branches;

	// Create workerCount threads
	#pragma omp parallel
	{
		int cycle_count = 0;
		int ID = omp_get_thread_num();

		// Seed each thread's PRNG for the select and randomise config options
		// srand(((int)time(NULL)) ^ rawID);
		// DO NOT PUSH only for profiling
		srand(ID);

		while (max_outer_loops_fixed < 0 || cycle_count < max_outer_loops_fixed) {
			++cycle_count;
			struct Result result = calculateOrder(ID, max_branches_fixed);

#if 0  // Disabled for profiling
			// result might store -1 frames for errors that might be recoverable
			if (result.frames > -1) {
				testRecord(result.frames);
			}
#endif
		}
	}

	// Statistics for profiling
	long sum = 0;
	for (int i = 0; i < workerCount; ++i) {
		printf("Thread %i iterations: %li\n", i, totalIterations[i]);
		sum += totalIterations[i];
	}
	printf("Total iterations: %li\n", sum);
	free(totalIterations);
	totalIterations = NULL;

	return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <libconfig.h>
#include "base.h"
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
#include "shutdown.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <omp.h>
#include "absl/base/port.h"
#if !_CIPES_IS_WINDOWS || defined(__MINGW32__)
#if _POSIX_C_SOURCE >= 200112L
#include <sys/select.h>
#endif
#include <unistd.h>
#include <errno.h>
#include <string.h>
#endif
#if _CIPES_IS_WINDOWS
#include <direct.h>
#endif

#define WAIT_TIME_BEFORE_CONTINUE_ON_FAILED_UPDATE_CHECK_SECS 10

#define UNSET_FRAME_RECORD 9999

int current_frame_record;
const char *local_ver;

// May get a value <0 if local record was corrupt.
int getLocalRecord() {
	int current_frame_record_orig = current_frame_record;
	if (ABSL_PREDICT_FALSE(current_frame_record < 0)) {
		printf("Current frame record is corrupt (less then 0 frames). Resetting (you may get false PBs for a while).\n");
		current_frame_record = UNSET_FRAME_RECORD;
		return current_frame_record_orig;
	}
	return current_frame_record;
}
void setLocalRecord(int frames) {
	if (ABSL_PREDICT_FALSE(frames < 0)) {
		printf("Got corrupt PB if %d frames. Ignoring\n", frames);
		return;
	}
	current_frame_record = frames;
}

const char *getLocalVersion() {
	return local_ver;
}

int numTimesExitRequest = 0;
#define NUM_TIMES_EXITED_BEFORE_HARD_QUIT 3

void countAndSetShutdown(bool isSignal) {
	if (++numTimesExitRequest >= NUM_TIMES_EXITED_BEFORE_HARD_QUIT) {
		if (!_CIPES_IS_WINDOWS || !isSignal) {
			printf("\nExit reqested %d times; shutting down now.\n", NUM_TIMES_EXITED_BEFORE_HARD_QUIT);
		}
		exit(1);
	} else {
		requestShutdown();
		if (!_CIPES_IS_WINDOWS || !isSignal) {
			printf("\nExit requested, finishing up work. Should shutdown soon (CTRL-C %d times total to force exit)\n", NUM_TIMES_EXITED_BEFORE_HARD_QUIT);
		}
	}
}

void handleTermSignal(int signum) {
	countAndSetShutdown(true);
}

void handleAbrtSignal(int signum) {
	if (signum != SIGABRT || (!_abrt_from_assert && signum == SIGABRT)) {
		printStackTraceF(stderr);
	}
	signal(signum, SIG_DFL);
	raise(signum);
}

/*void handleSegvSignale(int signal) {
 void *array[20];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 20);

	// print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", signal);
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(1);
}*/

#if _CIPES_IS_WINDOWS
BOOL WINAPI windowsCtrlCHandler(DWORD fdwCtrlType) {
	switch(fdwCtrlType) {
    case CTRL_C_EVENT: ABSL_FALLTHROUGH_INTENDED;
    case CTRL_CLOSE_EVENT:
      countAndSetShutdown(false);
      return TRUE;
    default:
    	return FALSE;
  }
}
#endif

void setSignalHandlers() {
	signal(SIGTERM, handleTermSignal);
	signal(SIGINT, handleTermSignal);
	signal(SIGABRT, handleAbrtSignal);
	signal(SIGSEGV, handleAbrtSignal);
	signal(SIGILL, handleAbrtSignal);
#ifdef SIGQUIT
	signal(SIGQUIT, handleAbrtSignal);
#endif
#ifdef SIGSYS
	signal(SIGSYS, handleAbrtSignal);
#endif
#if _CIPES_IS_WINDOWS
	if (!SetConsoleCtrlHandler(windowsCtrlCHandler, TRUE)) {
		printf("Unable to set CTRL-C handler. CTRL-C may cause unclean shutdown.\n");
	}
#endif
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

	printf("Welcome to Recipes@Home!\n");
	printf("Leave this program running as long as you want to search for new recipe orders.\n");

	current_frame_record = UNSET_FRAME_RECORD;
	initConfig();

	// If select and randomise are both 0, the same roadmap will be calculated on every thread, so set threads = 1
	// The debug setting can only be meaningfully used with one thread as well.
	int workerCount = (getConfigInt("select") || getConfigInt("randomise"))
					  && !getConfigInt("debug") ? getConfigInt("workerCount") : 1;
	local_ver = getConfigStr("Version");
	init_level_cfg();
	curl_global_init(CURL_GLOBAL_DEFAULT);	// Initialize libcurl
	int update = checkForUpdates(local_ver);
	
	if (update == -1) {
		printf("Could not check version on Github. Please check your internet connection.\n");
		printf("Otherwise, we can't submit completed roadmaps to the server!\n");
		printf("Alternatively you may have been rate-limited. Please wait a while and try again.\n");
#if _CIPES_IS_WINDOWS || _POSIX_C_SOURCE < 200112L
		printf("Will continue after %d seconds\n", WAIT_TIME_BEFORE_CONTINUE_ON_FAILED_UPDATE_CHECK_SECS);
		Sleep(WAIT_TIME_BEFORE_CONTINUE_ON_FAILED_UPDATE_CHECK_SECS * 1000);
#else
		fd_set stdin;
		FD_SET(STDERR_FILENO, &stdin);
		printf("Press ENTER to continue (will automatically continue in %d seconds).\n", WAIT_TIME_BEFORE_CONTINUE_ON_FAILED_UPDATE_CHECK_SECS);
		struct timeval tv;
		tv.tv_sec = WAIT_TIME_BEFORE_CONTINUE_ON_FAILED_UPDATE_CHECK_SECS;
		tv.tv_usec = 0;
		int retval = select(1, &stdin, NULL, NULL, &tv);
		if (retval == -1) {
			printf("Failure in waiting! %d (%s)\n", errno, strerror(errno));
			return retval;
		}
#endif
	}
	else if (update == 1) {
		printf("Please visit https://github.com/SevenChords/CipesAtHome/releases to download the newest version of this program!\n");
		printf("Press ENTER to quit.\n");
		char exitChar = getchar();
		return -1;
	}

	// Verify that username field is not malformed,
	// as this would cause errors when a roadmap is submitted to the servers
	if (getConfigStr("Username") == NULL) {
		printf("Username field is malformed. Please verify that your username is within quotation marks next to \"Username = \"\n");
		printf("Press ENTER to exit the program.\n");
		char exitChar = getchar();
		exit(1);
	}

	// Greeting message to user
	int blob_record = getFastestRecordOnBlob();
	if (blob_record == 0) {
		printf("There was an error contacting the server to retrieve the fastest time.\n");
		printf("Please check your internet connection, but we'll continue for now.\n");
	}
	else {
		printf("The current fastest record is %d frames.\n", blob_record);
	}

	// Verify that the results folder exists
	// If not, create the directory
#if _CIPES_IS_WINDOWS
	_mkdir("./results");
#else
	mkdir("./results", 0777);
#endif

	// To avoid generating roadmaps that are slower than the user's record best,
	// use PB.txt to identify the user's current best
	FILE* fp = fopen("results/PB.txt", "r");

	// The PB file may not have been created yet, so ignore the case where it is missing
	if (fp != NULL) {
		int PB_record;
		if (fscanf(fp, "%d", &PB_record) == 1) {
			if (PB_record < 0) {
				printf("PB.txt is corrupted (PB record less then 0 frames). Ignoring.\n");
			} else {
				current_frame_record = PB_record;
				if (current_frame_record < UNSET_FRAME_RECORD) {
					printf("Your current PB is %d frames.\n", current_frame_record);
				}
				testRecord(current_frame_record);
			}
		}
		fclose(fp);
		printf("Happy cooking!\n");

		// Submit the user's fastest roadmap to the server for leaderboard purposes
	}

	// Initialize global variables in calculator.c
	// This does not need to be done in parallel, as these globals will
	// persist through all parallel calls to calculator.c
	initializeInvFrames();
	initializeRecipeList();
	
	setSignalHandlers();
	
	// Create workerCount threads
	omp_set_num_threads(workerCount);

	// copying to const so OpenMP knows it doesn't have to have each thread
	// recheck this.
	const int max_outer_loops_fixed = max_outer_loops;
	const long max_branches_fixed = max_branches;

	#pragma omp parallel
	{
		long cycle_count = 0;
		int rawID = omp_get_thread_num();
		int displayID = rawID + 1;
		
		printf("[Thread %d/%d][Started]\n", displayID, workerCount);
		
		// Seed each thread's PRNG for the select and randomise config options
		srand(((int)time(NULL)) ^ rawID);
		
		while (max_outer_loops_fixed < 0 || cycle_count < max_outer_loops_fixed) {
			if (askedToShutdown()) {
				break;
			}
			++cycle_count;
			struct Result result = calculateOrder(rawID, max_branches_fixed);
			
			// result might store -1 frames for errors that might be recoverable
			if (result.frames > -1) {
				testRecord(result.frames);
			}
		}

		printf("[Thread %d/%d][Done]\n", displayID, workerCount);
	}
	
	return 0;
}

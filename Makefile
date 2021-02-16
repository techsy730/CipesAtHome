CFLAGS:=-lcurl -lconfig -fopenmp -Wall -I . -O2 $(CFLAGS)
HIGH_OPT_CFLAGS?=-O3
TARGET=recipesAtHome
DEPS=start.h inventory.h recipes.h config.h FTPManagement.h cJSON.h calculator.h logger.h shutdown.h $(wildcard absl/base/*.h)
OBJ=start.o inventory.o recipes.o config.o FTPManagement.o cJSON.o calculator.o logger.o shutdown.o
HIGH_PERF_OBJS=calculator.o inventory.o recipes.o

# Recognized configurable variables:
# CFLAGS=... : Any additional CFLAGS to be used (are specified after built in CFLAGS)
# HIGH_OPT_CLFLAGS=... : Any additional CFLAGS to pass to known CPU bottleneck source files
#   This overrides the default of "-O3" instead of appends to it
# USE_LTO=1 : Whether to enable link time optimizations (-flto)
#   Note that link time optimizations can be rather fiddly to get to work
# PROFILE_GENERATE=1 : Whether to enable profile generation for profile assited optimization
#   To actually generate the profile, you need to run the built recipesAtHome for a while. Ideally have a few new local records found and a few dozen branches searched before quitting (temporarily renaming your results folder may be helpfull for this)
# PROFILE_USE=1 : Whether to enable profile generation for profile assited optimization
#   MAKE SURE TO `make clean` FIRST before using after a PROFILE_GENERATE and generating the profile
# PERFORMANCE_PROFILING=1
#   Generate a binary ready to be profiled using gprov or similar
#   Unlike PROFILE_GENERATE which generatees profiles for profile assisted optimization, this option makes a binary ready for use for performance profiling.

RECOGNIZED_TRUE=1 true True TRUE yes Yes YES on On ON

LLVM_PROFDATA?=llvm-profdata
PROF_DIR?=prof
CLANG_PROF_MERGED?=$(TARGET).profdata

# .gcda and .gcno from GCC
# .profraw and .profdata from clang
# .dpi from ICC
KNOWN_PROFILE_DATA_EXTENSIONS=*.gcda *.gcno *.profraw *.profdata *.dpi

IS_CC_EXACTLY_CC=0
ifeq ($(CC),cc)
	IS_CC_EXACTLY_CC=1
endif
IS_CC_EMPTY=0
ifeq ($(CC),)
	IS_CC_EMPTY=1
endif


ifneq (,$(filter $(RECOGNIZED_YES), $(USE_LTO)))
	USE_LTO=1
endif
ifneq (,$(filter $(RECOGNIZED_YES), $(PROFILE_GENERATE)))
	PROFILE_GENERATE=1
endif
ifneq (,$(filter $(RECOGNIZED_YES), $(PROFILE_USE)))
	PROFILE_USE=1
endif
ifneq (,$(filter $(RECOGNIZED_YES), $(PERFORMANCE_PROFILING)))
	PERFORMANCE_PROFILING=1
endif


ifeq ($(PROFILE_GENERATE) $(PROFILE_USE), 1 1)
	$(error Cannot set both PROFILE_GENERATE and PROFILE_USE to true)
endif

UNAME:=$(shell uname)
ifneq ($(IS_CC_EXACTLY_CC) $(IS_CC_EMPTY), 0 0)
	ifeq ($(UNAME), Linux)
		CC=gcc
	endif
	ifeq ($(UNAME), Darwin)
		MACPREFIX:=$(shell brew --prefix)
		CC:=$(MACPREFIX)/opt/llvm/bin/clang
		CFLAGS:=-I$(MACPREFIX)/include -L$(MACPREFIX)/lib $(CFLAGS)
	endif
endif

ifneq (,$(findstring gcc,$(CC)))
	COMPILER=gcc
else ifneq (,$(findstring clang,$(CC)))
	COMPILER=clang
else
	COMPILER=unknown
endif

ifeq (1,$(USE_LTO))
	ifeq (gcc,$(COMPILER))
		CFLAGS+=-flto=jobserver -fuse-ld=gold
	else
		CFLAGS+=-flto
	endif
endif

ifeq (1,$(PERFORMANCE_PROFILING))
	CFLAGS+=-g -pg
	ifeq (gcc 1,$(COMPILER) $(USE_LTO))
		CFLAGS+=-ffat-lto-objects
	endif
endif
ifeq (1,$(PROFILE_GENERATE))
	ifeq (clang,$(COMPILER))
		CFLAGS+=-fcs-profile-generate=$(PROF_DIR)
	else
		ifneq (gcc,$(COMPILER))
			$(warning Unrecognized compiler "$(CC)". Profile generation might not work, disable "PROFILE_GENERATE" if you get build errors about unrecognized flags)
		endif
		CFLAGS+=-fprofile-generate=$(PROF_DIR)
	endif
endif

ifeq (1,$(PROFILE_GENERATE))
	MAKE_PROF_DIR_COMMAND=@mkdir -p $(PROF_DIR)
else
	MAKE_PROF_DIR_COMMAND=
endif

ifeq (1,$(PROFILE_USE))
	ifeq (clang,$(COMPILER))
		CFLAGS+=-fprofile-use=$(CLANG_PROF_MERGED)
	else
		ifeq (gcc,$(COMPILER))
			CFLAGS+=-fprofile-use=$(PROF_DIR) -fprofile-correction
		else
			CFLAGS+=-fprofile-use=$(PROF_DIR)
		endif
	endif
endif

ifeq (1 clang, $(PROFILE_USE) $(COMPILER))
	PROF_FINISH_COMMAND=$(LLVM_PROFDATA) merge -output=$(CLANG_PROF_MERGED) $(PROF_DIR)
else
	PROF_FINISH_COMMAND=
endif

default: $(TARGET)

.PHONY: clean clean_prof make_prof_dir prof_finish

make_prof_dir:
	$(MAKE_PROF_DIR_COMMAND)
	
prof_finish:
	$(PROF_FINISH_COMMAND)

$(HIGH_PERF_OBJS): %.o: %.c $(DEPS) | prof_finish
	$(CC) $(CFLAGS) $(HIGH_OPT_CFLAGS) -c -o $@ $<

%.o: %.c $(DEPS) | prof_finish
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET): $(OBJ) | make_prof_dir prof_finish
	$(CC) $(CFLAGS) $(HIGH_OPT_CFLAGS) -o $@ $^

clean:
	$(RM) ./*.o
	$(RM) ./$(TARGET)

ifeq (.,$(PROF_DIR))
clean_prof:
	$(RM) $(addprefix ./,$(KNOWN_PROFILE_DATA_EXTENSIONS))
	$(RM) $(CLANG_PROF_MERGED)
else
clean_prof:
	$(RM) $(addprefix ./,$(KNOWN_PROFILE_DATA_EXTENSIONS))
	$(RM) -r $(addprefix $(PROF_DIR)/,$(KNOWN_PROFILE_DATA_EXTENSIONS))
	$(RM) $(CLANG_PROF_MERGED)
endif


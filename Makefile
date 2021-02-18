# Warning, a reminder that 'make' is VERY HOSTILE to paths with spaces (separating argument parameters with spaces is fine).
# Not even escaping the space with "\ " will fix it.
# Just don't.

# PUT CONFIGURATION VARIABLES BEFORE THE make CALL (as environment variables for make)!
# Putting them after the make call (as arguments to make) causes make to IGNORE any changes we try to do.
# We do a LOT of heavy post processing of these flags, and will cause things to break horrifically if not able to.

CFLAGS:=-lcurl -lconfig -fopenmp -Wall -Werror=implicit-function-declaration -I . -O2 $(CFLAGS)
DEBUG_CFLAGS?=-g -fno-omit-frame-pointer -rdynamic
HIGH_OPT_CFLAGS?=-O3
TARGET=recipesAtHome
HEADERS=start.h inventory.h recipes.h config.h FTPManagement.h cJSON.h calculator.h logger.h shutdown.h $(wildcard absl/base/*.h)
OBJ=start.o inventory.o recipes.o config.o FTPManagement.o cJSON.o calculator.o logger.o shutdown.o
HIGH_PERF_OBJS=calculator.o inventory.o recipes.o

# Depend system inspired from http://make.mad-scientist.net/papers/advanced-auto-dependency-generation/
DEPDIR?=.deps
# gcc, clang, and icc all recognize this syntax
GCC_SYNTAX_DEP_FLAGS=-MT $@ -MMD -MP -MF $(DEPDIR)/$*.d

# Recognized configurable variables:
# DEBUG=1 Include debug symbols in the build and include stack traces (minimal to no impact on performance, just makes the binary bigger)
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
# USE_DEPENDENCY_FILES=1
#   Use the dependency file generation logic to only include headers needed by every C file to trigger a recompilation.
#   This requires generating Makefile snippet .d files under DEPDIR
#   Adds DEPFLAGS to the CFLAGS if true
#   If unset (or empty), this will be automatically chosen based on the compiler used.
#   If set to false (whether explicit or automatically), when any header file change will trigger recompilation of all source files.
# DEP_FLAGS
#   Add these flags to the CFLAGS to generate dependency files.
#   If unset, this will be automatically chosen based on the compiler used. 

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

DEBUG_EXPLICIT=0

ifneq (,$(filter $(RECOGNIZED_TRUE), $(USE_LTO)))
	USE_LTO=1
endif
ifneq (,$(filter $(RECOGNIZED_TRUE), $(PROFILE_GENERATE)))
	PROFILE_GENERATE=1
endif
ifneq (,$(filter $(RECOGNIZED_TRUE), $(PROFILE_USE)))
	PROFILE_USE=1
endif
ifneq (,$(filter $(RECOGNIZED_TRUE), $(PERFORMANCE_PROFILING)))
	PERFORMANCE_PROFILING=1
endif
ifneq (,$(filter $(RECOGNIZED_TRUE), $(DEBUG)))
	DEBUG=1
	DEBUG_EXPLICIT=1
endif
ifneq (,$(filter $(RECOGNIZED_TRUE), $(USE_DEPENDENCY_FILES)))
	USE_DEPENDENCY_FILES=1
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
		CFLAGS+=-I$(MACPREFIX)/include -L$(MACPREFIX)/lib $(CFLAGS)
	endif
endif

ifneq (,$(findstring gcc, $(CC)))
	COMPILER=gcc
else ifneq (,$(findstring g++, $(CC)))
	COMPILER=gcc
else ifneq (,$(findstring clang,$(CC)))
	COMPILER=clang
else ifneq (,$(filter icc icl icpc icx, $(CC)))
	COMPILER=icc
else
	COMPILER=unknown
endif

ifeq ($(COMPILER), $(filter gcc clang icc, $(COMPILER)))
_RECOGNIZES_STANDARD_DEPFLAGS:=1
endif

ifeq (,$(USE_DEPENDENCY_FILES))
	ifneq (,$(_RECOGNIZES_STANDARD_DEPFLAGS))
		USE_DEPENDENCY_FILES=1
	endif
endif
ifeq (1,$(USE_DEPENDENCY_FILES))
	ifneq (,$(_RECOGNIZES_STANDARD_DEPFLAGS))
		DEP_FLAGS?=$(GCC_SYNTAX_DEP_FLAGS)
	endif
	ifeq (,$(DEP_FLAGS))
		$(warning DEP_FLAGS is empty, but USE_DEPENDENCY_FILES=1; this will probably fail)
	endif
	DEPS:=$(DEPDIR)/%.d
	ifeq (,$(DEPDIR))
		MAKE_DEPDIR_COMMAND:=
	else ifeq (.,$(DEPDIR))
		MAKE_DEPDIR_COMMAND:=	
	else ifeq (./,$(DEPDIR))
		MAKE_DEPDIR_COMMAND:=
	else ifeq (.\,$(DEPDIR))
		MAKE_DEPDIR_COMMAND:=
	else
		MAKE_DEPDIR_COMMAND:=@mkdir -p $(DEPDIR)
	endif
else
	DEPS:=$(HEADERS)
	MAKE_DEPDIR_COMMAND:=
endif

ifeq (1,$(USE_LTO))
	DEBUG_CFLAGS+=-ffat-lto-objects
	ifeq (gcc,$(COMPILER))
		CFLAGS+=-flto=jobserver -fuse-ld=gold
	else
		CFLAGS+=-flto
	endif
endif

ifeq (1,$(PERFORMANCE_PROFILING))
	DEBUG?=1
	CFLAGS+=-pg
endif
ifeq (1,$(PROFILE_GENERATE))
	DEBUG?=1
	ifeq (clang,$(COMPILER))
		CFLAGS+=-fcs-profile-generate=$(PROF_DIR)
	else ifeq (gcc,$(COMPILER))
		CFLAGS+=-fprofile-generate=$(PROF_DIR) -fprofile-update=prefer-atomic
	else
		$(warning Unrecognized compiler "$(CC)". Profile generation might not work, disable "PROFILE_GENERATE" if you get build errors about unrecognized flags)
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

ifeq (1,$(DEBUG))
	CFLAGS+=$(DEBUG_CFLAGS)
	HIGH_OPT_CFLAGS+=$(DEBUG_CFLAGS)
endif

default: $(TARGET)

.PHONY: clean clean_prof prof_clean make_dep_dir make_prof_dir prof_finish

ifeq (,$(MAKE_DEPDIR_COMMAND))
make_dep_dir: ;
# $(MAKE_DEPDIR_COMMAND)
else
$(DEPDIR):
	$(info $(MAKE_DEPDIR_COMMAND))
	$(MAKE_DEPDIR_COMMAND)
make_dep_dir: $(DEPDIR)
endif

ifeq (,$(MAKE_PROF_DIR_COMMAND))
make_prof_dir: ;
else
$(PROF_DIR):
	$(MAKE_PROF_DIR_COMMAND)
make_prof_dir: $(PROF_DIR)
endif
	
prof_finish:
	$(PROF_FINISH_COMMAND)

# Delete the default c compile rules
%.o : %.c

$(HIGH_PERF_OBJS): %.o: %.c $(DEPS) | prof_finish make_dep_dir
	$(CC) $(DEP_FLAGS) $(CFLAGS) $(HIGH_OPT_CFLAGS) -c -o $@ $<

%.o: %.c $(DEPS) | prof_finish make_dep_dir
	$(CC) $(DEP_FLAGS) $(CFLAGS) -c -o $@ $<

$(TARGET): $(OBJ) | make_prof_dir prof_finish
	$(CC) $(CFLAGS) $(HIGH_OPT_CFLAGS) -o $@ $^

ifeq (,$(DEPDIR))
_DEPDIR_LOCATION=.
else
_DEPDIR_LOCATION=$(DEPDIR)
endif
DEPFILES:=$(patsubst %.c,$(_DEPDIR_LOCATION)/%.d,$(wildcard *.c))

# Have make ignore uncreated dep files if not generated yet
# If they are generated, the include $(wildcard $(DEPFILES)) will overwrite these rules.
$(DEPFILES):

clean:
	$(RM) ./*.o
	$(RM) ./$(TARGET)
	$(RM) ./*.d
	$(RM) ./$(DEPDIR)/*.d

_DO_REDUCED_CLEAN_PROF=0
ifeq (,$(PROF_DIR))
_DO_REDUCED_CLEAN_PROF=1
else ifeq (.,$(PROF_DIR))
_DO_REDUCED_CLEAN_PROF=1
else ifeq (./,$(PROF_DIR))
_DO_REDUCED_CLEAN_PROF=1
else ifeq (.\,$(PROF_DIR))
_DO_REDUCED_CLEAN_PROF=1
endif


ifeq (1,_DO_REDUCED_CLEAN_PROF)
clean_prof:
	$(RM) $(addprefix ./,$(KNOWN_PROFILE_DATA_EXTENSIONS))
	$(RM) $(CLANG_PROF_MERGED)
else
clean_prof:
	$(RM) $(addprefix ./,$(KNOWN_PROFILE_DATA_EXTENSIONS))
	$(RM) -r $(addprefix $(PROF_DIR)/,$(KNOWN_PROFILE_DATA_EXTENSIONS))
	$(RM) $(CLANG_PROF_MERGED)
endif

prof_clean: clean_prof

include $(wildcard $(DEPFILES))

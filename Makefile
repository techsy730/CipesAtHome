CFLAGS:=-lcurl -lconfig -fopenmp -Wall -I . -O2 $(CFLAGS)
HIGH_OPT_CFLAGS?=-O3
TARGET=recipesAtHome
DEPS=start.h inventory.h recipes.h config.h FTPManagement.h cJSON.h calculator.h logger.h shutdown.h $(wildcard absl/base/*.h)
OBJ=start.o inventory.o recipes.o config.o FTPManagement.o cJSON.o calculator.o logger.o shutdown.o
HIGH_PERF_OBJS=calculator.o inventory.o recipes.o

# Recognized configurable variables:
# CFLAGS : Any additional CFLAGS to be used (are specified after built in CFLAGS)
# HIGH_OPT_CLFLAGS : Any additional CFLAGS to pass to known CPU bottleneck source files
#   This overrides the default of "-O3" instead of appends to it
# USE_LTO=1 : Whether to enable link time optimizations (-flto)
#   Note that link time optimizations can be rather fiddly to get to work
# PROFILE_GENERATE=1 : Whether to enable profile generation for profile assited optimization
#   To actually generate the profile, you need to run the built recipesAtHome for a while. Ideally have a few new records found and a few dozen branches searched before quitting.
# PROFILE_USE=1 : Whether to enable profile generation for profile assited optimization
#   MAKE SURE TO `make clean` FIRST before using after a PROFILE_GENERATE and generating the profile

RECOGNIZED_TRUE=1 true True TRUE yes Yes YES on On ON

PROF_DIR?=prof

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


ifeq (1,$(PROFILE_GENERATE))
	CFLAGS+=-fprofile-generate=$(PROF_DIR)
endif

default: $(TARGET)

.PHONY: clean clean_prof make_prof_dir

ifeq (1,$(PROFILE_GENERATE))
make_prof_dir:
	@mkdir -p $(PROF_DIR)
else
make_prof_dir: ;
endif

ifeq (1,$(PROFILE_USE))
	CFLAGS+=-fprofile-use=$(PROF_DIR) -fprofile-correction
endif

$(HIGH_PERF_OBJS): %.o: %.c $(DEPS)
	$(CC) $(CFLAGS) $(HIGH_OPT_CFLAGS) -c -o $@ $<

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET): $(OBJ) | make_prof_dir
	$(CC) $(CFLAGS) $(HIGH_OPT_CFLAGS) -o $@ $^

clean:
	$(RM) ./*.o
	$(RM) ./$(TARGET)

ifeq (.,$(PROF_DIR))
clean_prof:
	$(RM) ./*.gcda
else
clean_prof:
	$(RM) ./*.gcda
	$(RM) -r $(PROF_DIR)/*.gcda
endif

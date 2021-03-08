# Warning, a reminder that 'make' is VERY HOSTILE to paths with spaces (separating argument parameters with spaces is fine).
# Not even escaping the space with "\ " will fix it.
# Just don't.

# PUT CONFIGURATION VARIABLES BEFORE THE make CALL (as environment variables for make)!
# Putting them after the make call (as arguments to make) causes make to IGNORE any changes we try to do.
# We do a LOT of heavy post processing of these flags, and will cause things to break horrifically if not able to.

WARNINGS_AND_ERRORS?=-Wall -Werror=implicit-function-declaration -Werror=implicit-int -Werror=incompatible-pointer-types -Werror=discarded-qualifiers -Werror=format-overflow -Werror=format-truncation -Werror=format-extra-args -Werror=format -Werror=maybe-uninitialized -Werror=array-bounds
CLANG_ONLY_WARNINGS?=-Wno-unused-command-line-argument -Wno-unknown-warning-option

EXTERNAL_LIBS=-lcurl -lconfig -fopenmp
CFLAGS:=-I . -O2 $(CFLAGS)
DEBUG_CFLAGS?=-g -fno-omit-frame-pointer -rdynamic
DEBUG_EXTRA_CFLAGS?=-DINCLUDE_STACK_TRACES=1 -DVERIFYING_SHIFTING_FUNCTIONS=1
DEBUG_VERIFY_PROFILING_CFLAGS?=
HIGH_OPT_CFLAGS?=-O3
GCC_ONLY_HIGH_OPT_CFLAGS?=
# Trying to match x86-64-v3
# as specified in https://www.phoronix.com/scan.php?page=news_item&px=GCC-11-x86-64-Feature-Levels
# An intersection of -march=haswell and -march=bdver4 (as of gcc-9) is the most concise way to express this (until gcc-11 and clang-12)
AVX2_BUILD_CFLAGS?=-march=haswell -mno-hle -mtune=generic
EXPERIMENTAL_OPT_CFLAGS?=-DENABLE_PREFETCHING=1
FAST_CFLAGS_BUT_NO_VERIFY?=-DNO_MALLOC_CHECK=1 -DNDEBUG -DFAST_BUT_NO_VERIFY=1
GCC_ONLY_FAST_CFLAGS_BUT_NO_VERIFY?=-fno-stack-protector -fno-stack-check -fno-sanitize=all
CLANG_ONLY_FAST_CFLAGS_BUT_NO_VERIFY?=-fno-stack-protector -fno-stack-check -fno-sanitize=all
TARGET=recipesAtHome
HEADERS=start.h inventory.h recipes.h config.h FTPManagement.h cJSON.h calculator.h logger.h shutdown.h base.h internal/base_essentials.h internal/base_asserts.h semver.h stacktrace.h $(wildcard absl/base/*.h)
OBJ=start.o inventory.o recipes.o config.o FTPManagement.o cJSON.o calculator.o logger.o shutdown.o base.o semver.o stacktrace.o
HIGH_PERF_OBJS=calculator.o inventory.o recipes.o

# Depend system inspired from http://make.mad-scientist.net/papers/advanced-auto-dependency-generation/
DEPDIR?=.deps
# gcc, clang, and icc all recognize this syntax
GCC_SYNTAX_DEP_FLAGS=-MT $@ -MMD -MP -MF $(DEPDIR)/$*.d

# Recognized configurable variables: (any of the ones listed as "=1" don't actually default to 1, that is what you set it to enable the feature)
# DEBUG=1 : Include debug symbols in the build and include stack traces (minimal to no impact on performance, just makes the binary bigger)
# CFLAGS=... : Any additional CFLAGS to be used (are specified after built in CFLAGS)
# HIGH_OPT_CLFLAGS=... : Any additional CFLAGS to pass to known CPU bottleneck source files
#   This overrides the default of "-O3" instead of appends to it
# EXPERIMENTAL_OPTIMIZATIONS=1 :  Include some experimental optimizations that may or may not actually improve performance;
#  may cause some code to be uneligable for certain optimizations that might offset any benefits that code would get. 
# FAST_CFLAGS_BUT_NO_VERIFICATION=1 : Disable some forms of verifiation that should allow it to optimize even better,
#   but you lose most protection against undefined behavior if something goes wrong.
# USE_LTO=1 : Whether to enable link time optimizations (-flto)
#   Note that link time optimizations can be rather fiddly to get to work
# PROFILE_GENERATE=1 : Whether to enable profile generation for profile assited optimization
#   To actually generate the profile, you need to run the built recipesAtHome for a while. Ideally have a few new local records found and a few dozen branches searched before quitting (temporarily renaming your results folder may be helpfull for this)
# PROFILE_USE=1 : Whether to enable profile generation for profile assited optimization
#   MAKE SURE TO `make clean` FIRST before using after a PROFILE_GENERATE and generating the profile
# PERFORMANCE_PROFILING=1
#   Generate a binary ready to be profiled using gprov or similar
#   Unlike PROFILE_GENERATE which generatees profiles for profile assisted optimization, this option makes a binary ready for use for performance profiling.
#   You may want to consider setting DEBUG_VERIFY_PROFILING=1 as well if you want to have even more things like heap validation.
# DEBUG_EXTRA=1 : Include even more diagnostic messages but may lose out on some small optimization opportunities
# DEBUG_VERIFY_PROFILING=1 Extra debugging flags to enable for things like verifying heap integrity and performance profiling (WILL reduce performance)
# USE_GOOGLE_PERFTOOLS=1
#   Use Google's perftools (and malloc implementation).
#   For Ubuntu, you need to install the packages
#     google-perftools, libgoogle-perftools-dev
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
ifneq (,$(filter $(RECOGNIZED_TRUE), $(EXPERIMENTAL_OPTIMIZATIONS)))
	EXPERIMENTAL_OPTIMIZATIONS=1
endif
ifneq (,$(filter $(RECOGNIZED_TRUE), $(FAST_CFLAGS_BUT_NO_VERIFICATION)))
	FAST_CFLAGS_BUT_NO_VERIFICATION=1
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
ifneq (,$(filter $(RECOGNIZED_TRUE), $(DEBUG_EXTRA)))
	DEBUG_EXTRA=1
	DEBUG=1
	DEBUG_EXPLICIT=1
endif
ifneq (,$(filter $(RECOGNIZED_TRUE), $(DEBUG_VERIFY_PROFILING)))
	DEBUG_VERIFY_PROFILING=1
endif
ifneq (,$(filter $(RECOGNIZED_TRUE), $(USE_GOOGLE_PERFTOOLS)))
	USE_GOOGLE_PERFTOOLS=1
endif
ifneq (,$(filter $(RECOGNIZED_TRUE), $(USE_DEPENDENCY_FILES)))
	USE_DEPENDENCY_FILES=1
endif
ifneq (,$(filter $(RECOGNIZED_TRUE), $(FOR_DISTRIBUTION)))
	FOR_DISTRIBUTION=1
endif
ifneq (,$(filter $(RECOGNIZED_TRUE), $(ASSUME_X86_64_V3)))
	# See https://www.phoronix.com/scan.php?page=news_item&px=GCC-11-x86-64-Feature-Levels
	ASSUME_X86_64_V3=1
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

_64_BIT_MINGW=x86_64-w64-mingw
_32_BIT_MINGW=i686-w64-mingw
_HAS_M64_CFLAG=$(findstring -m64,$(CFLAGS))
_HAS_M32_CFLAG=$(findstring -m32,$(CFLAGS))

IS_WINDOWS=0
IS_WINDOWS_32=0
IS_WINDOWS_64=0

ifneq (,$(and $(findstring $(_32_BIT_MINGW),$(CC)),$(_HAS_M64_CFLAG)))
$(error Can't use "-m64" on a 32-bit MinGW compiler. Use the right compiler for bitness (should start with $(_64_BIT_MINGW)))
else ifneq (,$(and $(findstring $(_64_BIT_MINGW),$(CC)),$(_HAS_M32_CFLAG)))
$(error Can't use "-m32" on a 64-bit MinGW compiler. Use the right compiler for bitness (should start with $(_32_BIT_MINGW)))
endif

ifneq (,$(findstring $(_64_BIT_MINGW),$(CC)))
	COMPILER=gcc
	IS_WINDOWS=1
	IS_WINDOWS_64=1
else ifneq (,$(findstring $(_32_BIT_MINGW),$(CC))) 
	COMPILER=gcc
	IS_WINDOWS=1
	IS_WINDOWS_32=1
else ifneq (,$(findstring gcc, $(CC)))
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

ifeq (clang,$(COMPILER))
	WARNINGS_AND_ERRORS:=$(CLANG_ONLY_WARNINGS) $(WARNINGS_AND_ERRORS)
endif
CFLAGS:=$(WARNINGS_AND_ERRORS) $(CFLAGS)

ifeq (1,$(FOR_DISTRIBUTION))
ifeq (1,$(IS_WINDOWS))
	# FINAL_STATIC_LINKS+=
	# FINAL_TARGET_CFLAGS+=
	EXTERNAL_LIBS+=-Iinclude_manually_provided -Llib_manually_provided/win
endif
ifeq (1,$(IS_WINDOWS_64))
	ifeq (1 true,$(ASSUME_X86_64_V3) \
	$(shell if [ -d lib_manually_provided/win64-avx2 -a -f lib_manually_provided/win64-avx2/libcurl.dll ]; then echo "true"; fi))
		EXTERNAL_LIBS+=-Llib_manually_provided/win64-avx2
	else
		EXTERNAL_LIBS+=-Llib_manually_provided/win64
	endif
endif
ifeq (1,$(IS_WINDOWS_32))
	EXTERNAL_LIBS+=-Llib_manually_provided/win32
	# Format strings for size_t don't line up, and not really an easy way to fix it.
	# Demote mismatching args to a warning
	CFLAGS+=--warn-format
endif
endif

ifeq (1,$(IS_WINDOWS_32))
	# Format strings for size_t don't line up, and not really an easy way to fix it.
	# Demote mismatching args to a warning
	CFLAGS:=$(filter-out -Werror=format,$(CFLAGS))
endif

CFLAGS:=$(EXTERNAL_LIBS) $(CFLAGS)

ifeq (1,$(FOR_DISTRIBUTION))
ifeq (1,$(ASSUME_X86_64_V3))
	CFLAGS:=$(AVX2_BUILD_CFLAGS) $(CFLAGS)
endif
endif

ifeq (gcc,$(COMPILER))
	HIGH_OPT_CFLAGS+=$(GCC_ONLY_HIGH_OPT_CFLAGS)
endif
ifeq (1,$(FAST_CFLAGS_BUT_NO_VERIFICATION))
	CFLAGS+=$(FAST_CFLAGS_BUT_NO_VERIFY)
	ifeq (gcc,$(COMPILER))
		CFLAGS+=$(GCC_ONLY_FAST_CFLAGS_BUT_NO_VERIFY)
	else ifeq (clang,$(COMPILER))
		CFLAGS+=$(CLANG_ONLY_FAST_CFLAGS_BUT_NO_VERIFY)
	endif
endif
ifeq (1,$(EXPERIMENTAL_OPTIMIZATIONS))
	CFLAGS+=$(EXPERIMENTAL_OPT_CFLAGS)
endif

ifeq (1,$(USE_GOOGLE_PERFTOOLS))
	ifeq (1,$(PERFORMANCE_PROFILING))
		DEBUG?=1
		CFLAGS:= $(CFLAGS) -ltcmalloc_and_profiler
	else ifeq (1,$(DEBUG))
		CFLAGS:=$(CFLAGS) -ltcmalloc
	else
		CFLAGS:=$(CFLAGS) -ltcmalloc_minimal
	endif
else
	ifeq (1,$(PERFORMANCE_PROFILING))
		DEBUG?=1
		CFLAGS+=-pg
	endif
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
	ifeq (gcc,$(COMPILER))
		DEBUG_CFLAGS+=-ffat-lto-objects
		CFLAGS+=-fuse-ld=gold -flto=jobserver
	else
		CFLAGS+=-flto
	endif
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
	ifeq (1,$(DEBUG_VERIFY_PROFILING))
		ifeq (gcc 0,$(COMPILER) $(USE_GOOGLE_PERFTOOLS))
			DEBUG_VERIFY_PROFILING_CFLAGS+=-static-libasan -fsanitize=address
		endif
		DEBUG_CFLAGS+=$(DEBUG_VERIFY_PROFILING_CFLAGS)
	endif
	ifeq (1,$(DEBUG_EXTRA))
		DEBUG_CFLAGS+=$(DEBUG_EXTRA_CFLAGS)
		ifeq (1,$(IS_WINDOWS))
			# EXTERNAL_LIBS+=-
		endif
	endif
	ifeq (1,$(IS_WINDOWS))
		DEBUG_CFLAGS:=$(filter-out -rdynamic,$(DEBUG_CFLAGS))
	endif
	CFLAGS+=$(DEBUG_CFLAGS)
	HIGH_OPT_CFLAGS+=$(DEBUG_CFLAGS)
endif

default: $(TARGET)

all: default

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
	$(CC) $(CFLAGS) $(HIGH_OPT_CFLAGS) $(FINAL_TARGET_CFLAGS) -o $@ $^ $(FINAL_STATIC_LINKS)

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
	$(RM) ./$(TARGET) ./$(TARGET).exe
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

# CipesAtHome-TechSY730

## TechSY730's modifications

**WARNING**: I have _not_ tested this in Visual Studio on Windows, as I don't have Visual Studio or Windows.

If you find any issues with my fork, you can file them on https://github.com/techsy730/CipesAtHome

* A bunch of memory leaks fixed
* Compile the "inner loop" C (currently `caliculator.c`, `recipes.c`, and `inventory.c`) files with `-O3`.
* A bunch of optimizations, especially reducing malloc churn
* Cleaner shutdown, give the threads a chance to finish their work before shutting down (you can force it by giving the exit command (`ctrl-C` usually) multiple times.
* Better (IMO) logging
* More Makefile configuration options

### Branches

There are three branches to choose from.

* MakefileImprovements: Only the tamer `Makefile` adjustments and basic code changes to support clean shutdowns.
* Optimizations: The `MakefileImprovements` branch and also a bunch optimizations and fixes to memory leaks (also more even more fancy Makefile options)
* ConstantsAdjustments: The `MakefileImprovements` branch as well as a bunch of tuning constant additions and
  changes to (hopefully) have `recipesAtHome` spend less time on probably dead-end branches.
  You can change these yourself if you wish by editing the `#define`s in calculator.c.
  
`master` is currently tracking `ConstantsAdjustments`.

### New Makefile configuration variables

Make sure to append these _before_ the `make` call (using env or something). Passing these into `make`'s parameters causes `make`
to override these.

**Note**: This obviously only works when using `make`. if you are building through Visual Studio, you won't get any of this.

This doesn't list all of them, but some of the ones normal users might be interested in.
You can see all the important ones in the `Makefile`

* `CC=...`: Override the compiler used to this one. So you can now use `clang` on Linux, or a newer compiler then your OS's
  default selection, among other uses.
* `CFLAGS=...`: You can use this to add your own CLFAGS on top of what CipesAtHome needs to build.
  One common use is to set `-march=native` or similar to build a binary taking advantage of all the features
  your processor has, at the expense of portabability (so don't send the build executable to anyone else).
* `USE_LTO=1`: Turn on link time optimizations, which should allow the compiler far more whole program optimization potential
  This can be a bit fiddly on some compilers and linker combinations though.
* `DEBUG=1`: Include debug symbols in the binary, for easier debugging if something goes wrong.
  This will not slow down execution by any meaningful degree, but will increase binary size.<br>
  There is also `DEBUG_EXTRA=1` for even more output on failures, but can lose performance indirectly
  by making some code ineligible for certain optimizations.
* `FAST_CFLAGS_BUT_NO_VERIFICATION=1`: Turn off a bunch of runtime validation assertions, so there is much
  less protection against undefined behavior if anything goes wrong with the code, and very little output about
  what went wrong if it does.
* `USE_GOOGLE_PERFTOOLS=1`: Use [Gperfutils](https://github.com/gperftools/gperftools) (formerly "Google Performance Tools")
  and their `malloc` implementation that tends to work much better in multi-core contexts due to thread local heap pools.<br>
  Currently Gperfutils is only available for Linux.<br>
  On Ubuntu at least, you can install this using `apt install google-perftools libgoogle-perftools-dev`<br>
  Note in this package, `pprof` is instead names `google-pprof`. Otherwise it acts the same.
* `PROFILE_GENERATE=1`: Generate the instrumentation profile. See [Profile Guided Optimization](#profile_guided_optimizaton)

  **Note**: You may want to run `make prof_clean` to clean our the profiling data from previous runs before
  running the generated binary.
* `PROFILE_USE=1`: Use the generated profile data to optimize even further. See [Profile Guided Optimization](#profile_guided_optimizaton)

  **Warning**: You _must_ `make clean` before doing this or else `make` won't recognize anything really
  changed and thus not rebuild using the profile.

There are also options to enable profiling profiling (as in for memory tracking and CPU time tracking) options, see the `Makefile`
for more details.

<a name=profile_guided_optimizaton></a>

### Profile Guided Optimization

Profile Guided Optimization (PGO) is the act of using data about a run's counts of functions and branch paths in order to have
a better understand what parts of the program are "hot" or "cold" ("used a lot" or "not used much"), and then having the
compiler make an optimized binary that use this knowledge for better optimization.

#### Generating the instrumentation profile `PROFILE_GENERATE=1`

For CipesAtHome, you can accomplish this by building with `PROFILE_GENERATE=1`, which will generate an intstrumented binary.
This binary will dump instrumentation data (the "hot" vs "cold" data mentioned above) to files in a subfolder called `prof/`
(the exact files in the folder will depend on your compiler).

Before running the built `recipesAtHome`, I suggest running `make prof_clean` to clean out any previous profiling data
(some compilers will refuse to overwrite it if there are old files there).

Furthermore, I suggest starting from a fresh `results` folder, so some "PB finding" code gets covered as well.
So move your `results` folder to `results.BAK` or something.

Now run `recipesAtHome`. It *will* run **much** slower then an optimized build; this is normal, it has to dump profiling data while running.
This should be multithread safe for `gcc` and `clang`
I would let it run for about 30 minutes or so.

After this, _make sure_ to `make clean` or `make` won't know to rebuild these files.
**Do not** run `make prof_clean`, as that will remove the profiling data you just gathered.

#### Using the profiled data `PROFILE_USE=1`

Then rebuild using `PROFILE_USE=1` (and unset `PROFILE_GENERATE` or set it to `0`), and the profiling data should be
picked up and you will get an even more optimized binary.

Make sure to copy (I highly suggest copy, not move, to keep a backup) `results.BAK` to `results`.
Now run your

### Building/Releasing

#### Releasing for Windows using MinGW-w64

Since I personally do not have Visual Studio or Windows, I have to compile for Windows using MinGW-w64 (note the w64 part, it is a fork of the original MinGW).

If you DO have Visual Studio, then I strongly encourage you to follow the [original Windows instructions](#windows-building-original)

My workflow is very specific to my setup. You will probably need to change compiler names.

I still follow [original Windows instructions](#windows-building-original).

However, _before_ building I set

```
export CFLAGS="-O2 -flto=jobserver -fuse-ld=gold -ftree-vectorize"
export CXXFLAGS="$CFLAGS"
# Customize these based on what your distribution names them as
CC_64=x86_64-w64-mingw32-gcc
CXX_64=x86_64-w64-mingw32-g++
CC_32=i686-w64-mingw32-gcc
CXX_32=i686-w64-mingw32-g++
```

Then build using

```
# 64-bit build
export CC=$CC_64
export CXX=$_64
vcpkg --triplet x64-mingw-dynamic install curl
vcpkg --triplet x64-mingw-dynamic install libconfig --head 
# 32-bit build (--no-downloads to prevent the case of another version being downloaded and having inconsistent versions)
export CC=CC_32
export CXX=CC_32
vcpkg --triplet x86-mingw-dynamic install --no-downloads curl
vcpkg --triplet x86-mingw-dynamic install --no-downloads libconfig --head
unset CC CXX
```

For gathering the prebuilt dlls and artifacts, I use the included script `update-windows-shared-libs.sh`, which basically does the below.

```
# Copying the correct DLLs for the right bitness of MinGW. update-windows-shared-libs.sh takes care of all that mess for you.

cp "$VCPKG_ROOT/packages/curl_x64-mingw-dynamic/bin/libcurl.dll" lib_manually_provided/win64/
cp "$VCPKG_ROOT/packages/libconfig_x64-mingw-dynamic/bin/libconfig.dll" lib_manually_provided/win64/
cp "$VCPKG_ROOT/packages/zlib_x64-mingw-dynamic/bin/libzlib1.dll" lib_manually_provided/win64/
cp "$VCPKG_ROOT/packages/openssl_x64-mingw-dynamic/bin/libssl-1_1-x64.dll" lib_manually_provided/win64/
cp "$VCPKG_ROOT/packages/openssl_x64-mingw-dynamic/bin/libcrypto-1_1-x64.dll" lib_manually_provided/win64/

cp "$VCPKG_ROOT/packages/curl_x86-mingw-dynamic/bin/libcurl.dll" lib_manually_provided/win32/
cp "$VCPKG_ROOT/packages/libconfig_x86-mingw-dynamic/bin/libconfig.dll" lib_manually_provided/win32/
cp "$VCPKG_ROOT/packages/zlib_x86-mingw-dynamic/bin/libzlib1.dll" lib_manually_provided/win32/
cp "$VCPKG_ROOT/packages/openssl_x86-mingw-dynamic/bin/libssl-1_1.dll" lib_manually_provided/win32/
cp "$VCPKG_ROOT/packages/openssl_x86-mingw-dynamic/bin/libcrypto-1_1.dll" lib_manually_provided/win32/

# Headers are the same for 32-bit and 64-bit, doesn't matter which we choose from
cp "$VCPKG_ROOT/packages/libconfig_x64-mingw-dynamic/include/libconfig.h" include_manually_provided/
mkdir -p include_manually_provided/curl
cp "$VCPKG_ROOT/packages/curl_x64-mingw-dynamic/include/curl/"*.h include_manually_provided/curl/
```

Then build CipesAtHome
Unset CFLAGS and CXXFLAGS set above, as they are not appropriate for this Makefile.
If you are doing this from a different 

```
# Skip this if your CFLAGS and CXXFLAGS aren't from the above vcpkg building
unset CFLAGS CXXFLAGS

# 64-bit build
make clean
env CC=$CC_64 FOR_DISTRIBUTION=1 USE_LTO=1 make -j4
# Do whatever you need to do to package for Windows 64-bit release...

# 32-bit
make clean
env CC=$CC_32 FOR_DISTRIBUTION=1 CFLAGS="-m32" USE_LTO=1 make -j4
# Do whatever you need to do to package for Windows 32-bit release...
```

The "-avx2" build requires even further modifications and is not documented at this time. This may change in the future.

# CipesAtHome (Original README.md starts here)

## Overview
Welcome to CipesAtHome! This is a depth-first-search brute-forcer which implements different RNG-based methodologies to iterate down unique paths in the recipe tree in an attempt to discover the fastest known order of recipes. Upon discovery of the fastest known recipe roadmap, a generated roadmap file is submitted to a Blob server that we maintain. Upon reaching the first Zess T section in the Paper Mario: TTYD 100% TAS, the search will be over and the TAS will implement the currently known fastest recipe roadmap. Below are a set of rules and assumptions regarding a glitch we will be making use of called Inventory Overload. Given these rules and assumptions, we have constructed an algorithm which mimics how the game handles the inventory while under the effect of this glitch, as well as determine whether or not certain moves can be performed, which may include: cooking a particular recipe, using a particular item combination to cook a particular recipe, and tossing a particular item in the inventory to make room for the output.

## Rules and Assumptions
- A glitch is being exploited that allows one to set the number of available inventory slots in the inventory to ten after it was already expanded to twenty and had items in those slots.
- Using an item in the first ten slots removes the item, and the slot becomes unfilled, as happens normally.
- However, using an item in the last ten slots does not remove it, effectively allowing for duplication.
- Sorting of the inventory is still fully functional, so any item can be duplicated this way by sorting it to a slot between 11-20.
- While the glitch is active, unfilled slots in the first ten are moved to what the game thinks is the end of the inventory. So a newly vacated slot is moved to slot 10, with everything between the previous position of the slot and slot 10 being moved one slot towards the beginning of the inventory. In the pause menu, these slots are displayed as NULL  items, but in the menus for giving or replacing an item, they do not show up.
- When the game displays the inventory, it counts how many non-NULL items are in the inventory. It then displays the items in that many slots starting from the beginning. This is why NULL items are normally not displayed, since they are in the slots that are not displayed. However, due to the glitch placing the NULLs in the middle of the inventory, the slots that are not shown actually have items in them. So effectively, there are as many hidden items at the end of the inventory as there are NULLs. In order to use these items, the NULL slots must be either filled or moved to the actual end of the inventory with a sort.
- When there is at least one NULL slot, receiving an item causes the inventory items preceding the first NULL slot to be shifted one position towards the back of the inventory. The received item is then placed at the start. (This is what happens regardless of the inventory glitch.)
- When there are no NULL slots, receiving an item gives the player the option of tossing either the item produced or an item in the inventory. Due to the glitch, attempting to toss an item in the second half of the inventory will not replace the item, and the item received will be lost. Therefore, the second half of the inventory is never considered in this decision.
- When the inventory is sorted, the items are placed in the specified order starting from the beginning of the inventory. All NULL slots are therefore no longer available, since they are now all at the end of the inventory. The total number of slots available to work with can therefore decrease over time, and once that has occurred, it is permanent. The unavailable slots at the end of the inventory are referred to as BLOCKED.
- Due to the progression of Zess T.'s dialogue, one recipe is cooked before giving her the cookbook. This cookbook is what allows her to cook recipes with two ingredients, so the first recipe that is fulfilled can only use a single ingredient.
- The glitch greatly reduces the need for obtaining items to use to cook recipes. In fact, by placing most item uses before starting to cook recipes, it is almost possible to complete them all in one trip to Zess T. Unfortunately, the Dried Bouquet is a required item for one recipe, and in order to get it, the Mousse Cake, which is cooked by Zess T., must already be possessed. The Dried Bouquet is unique, so the inevitable conclusion is that there must be two trips to Zess T., with an intermission between them.
- In the intermission, two Hot Dogs and a Mousse Cake are traded for a Dried Bouquet. Then, a Coconut and a Keel Mango are collected, and the Coconut is traded to Flavio for the Chuckola Cola. The Courage Shell is collected, and a Thunder Rage is used to fight Smorg. All of the collected items (except for the Chuckola Cola, which is an Important Item) are required for making recipes, which means that a sort is required between obtaining the Coconut and giving it to Flavio in order to keep it using the glitch. Other sorts are possible during the intermission as well, but the only one that will be considered is the one that is required.

## Algorithm Description
- Start main loop.
- Iterate through all currently possible ways of making the recipes that have not been fulfilled.
- Iterate through all placement options of the output of the recipe.
- Calculate the frames to perform the action and add it to a list of "legal moves" for the current state, sorted ascending by frames.
- If the Chapter 5 intermission can be performed, add every way that it can be completed with one sort to the legal move list.
- If the last move was not a sort, add the four ways of sorting to the end of the legal move list, regardless of frame count.
- Use RNG-based methodologies to select a legal move from the list, set this legal move as the current state, and begin the main loop a level deeper.
- If there are no legal moves remaining, then go back up a level and pick the next legal move to explore.
- Periodically reset the search space to get a new random branch. This prevents the user from extensively searching a dead-end branch.

## Building
### Linux
To build on Linux, you will need to run the following commands to run this program:
1. `sudo apt-get install make libcurl4-openssl-dev libomp-dev libconfig-dev`
1. `make`
1. `./recipesAtHome`

Should there be any problems in this building process, please let us know by posting an issue on Github.

<a name="building-windows-original"></a>

### Windows
To build on Windows, use the Visual Studio CipesAtHome.sln solution file. You will need to install libcurl and libconfig. This can be done by making use of vcpkg.

### macOS
To build on macOS (w/ Homebrew installed), you will need to run the following commands from the project root, in order to run this program:
1. `brew install llvm libomp libconfig`
1. `make`
1. `./recipesAtHome`

To run the pre-built unix executable on mac:
1. download and unzip the file
1. open a new terminal window
1. `cd ` followed by the path to the extracted folder
1. `./recipesAtHome`

Should there be any problems in the building process, please let us know by posting an issue on Github.

## Config Settings
Below are a set of config parameters which can be changed by the user. These will affect how the algorithm handles legal moves, as well as check for the current program version on Github.

- **select**: This is a methodology that chooses the <em>i</em>th legal move in the array with arbitrary probability (0.5)<sup><em>i</em></sup>. This is used to effectively generate a different roadmap final roadmap on every descent.
- **randomise**: This methodology randomises the entire list of legal moves. This will not generate fast roadmaps as efficiently as select will be. In the event that select and randomise are both chosen, the algorithm will prioritize the select methodology.
- **logLevel**: This parameter specifies the degree of detail to which the program will output log information both to stdout and to the log file. The higher the number, the more detail will be output. If logLevel is 0, no data will be output. Specific thresholds are listed in config.txt.
- **workerCount**: The number of threads to run simultaneously for the program. Generally, this can be as high as (# CPU Cores) - 2, though increasing the workerCount means that less CPU time can be dedicated to other programs running on your computer. If you are running any intensive program besides this program, then you should close the program, change the workerCount, and restart the program while using the other intensive program.
- **Username**: This name will be submitted to the server to specify who found the roadmap. If you would like to be known for finding the fastest roadmap, change this name to a username of your choice. This is limited by 19 characters. A Discord bot in the TTYD speedrunning server will alert us with this Username when a new fastest roadmap is found.

## Docker Setup
If your system is set up with Docker, you can quickly run CipesAtHome with a Docker image:
1. Mount the `/config` directory, set any environment variables, and run the container:
   - `docker run -e USERNAME=MyName -v /my/volume/location/cipesathome:/config sevenchords/cipesathome`
1. On first run, a config.txt will automatically be created. Feel free to modify it, as it will be used on the next startup.
   - If you're having issues with the config file, check its filesystem permissions and make sure it ends with a newline

### Docker Environment Variables
Available variables:
`SELECT`, `RANDOMISE`, `LOG_LEVEL`, `BRANCH_LOG_INTERVAL`, `WORKER_COUNT`, `USERNAME`

Environment variables are used to set `config.txt` values on first run. If config.txt already exists in the volume, the environment variables will do nothing.

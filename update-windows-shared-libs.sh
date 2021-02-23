#!/bin/bash

update-windows-shared-libs() {
	if [[ "$PWD" != *CipesAtHome && "$REALLY_AM_IN_RIGHT_DIR" != 1 ]]; then
		printf "This command must be run in the root of the CipesAtHome checkout.\n"
		printf "If you are really in the right directory and it is just a different name, set \"REALLY_AM_IN_RIGHT_DIR\" to 1"
		exit 1
	fi
		
	local VCPKG_ROOT="$VCPKG_ROOT"
	if [ $# -ge 1 ]; then
		VCPKG_ROOT="$1"
	fi
	
	if [ -z "$VCPKG_ROOT" ]; then
		printf "You must specify \"VCPKG_ROOT\" to be the directory of your vcpkg installation, or you can give it as the first parameter to this script.\n"
	fi
	
	[[ -d lib_manually_provided/win64-avx2 && -f "$VCPKG_ROOT/packages/curl_x64-v3-mingw-dynamic/bin/libcurl.dll" ]]
	USING_AVX2_BUILD=$?
	
	OPEN_MP_DLLS=("libgomp-1.dll" "libwinpthread-1.dll")

	if [ -z "${CC}" ]; then
		CC="${CC_64}"
	fi
	if [ -z "${CC_32}" ]; then
		CC_32="${CC#x86_64}"
		CC_32="i686${CC_32}"
	fi
	# Print out commands as they are run and abort on first failure	
	local -a OPEN_MP_DLL_LOCS_64
	local -a OPEN_MP_DLL_LOCS_64_AVX2
	local -a OPEN_MP_DLL_LOCS_32

	set -e
	for x in "${OPEN_MP_DLLS[@]}"; do 
		OPEN_MP_DLL_LOCS_64+=($("${CC}" --print-file-name="$x"))
		OPEN_MP_DLL_LOCS_32+=($("${CC_32}" --print-file-name="$x"))
	done
	# Two bitness specific libs
  OPEN_MP_DLL_LOCS_64+=($("${CC}" --print-file-name="libgcc_s_seh-1.dll"))
  OPEN_MP_DLL_LOCS_32+=($("${CC_32}" --print-file-name="libgcc_s_sjlj-1.dll"))
  
  set +e
	# Remember in shell land, 0 means true!
  if [[ $USING_AVX2_BUILD == 0 ]]; then
  	set -e
		for x in "${OPEN_MP_DLLS[@]}"; do 
			OPEN_MP_DLL_LOCS_64_AVX2+=($("${CC}" -march=haswell -mno-hle --print-file-name="$x"))
		done
	  OPEN_MP_DLL_LOCS_64_AVX2+=($("${CC}" -march=haswell -mno-hle --print-file-name="libgcc_s_seh-1.dll"))
  fi
	set -xe
	for x in "${OPEN_MP_DLL_LOCS_64[@]}"; do 
		cp "$x" lib_manually_provided/win64/
	done
	for x in "${OPEN_MP_DLL_LOCS_32[@]}"; do 
		cp "$x" lib_manually_provided/win32/
	done
	set +e
	if [[ $USING_AVX2_BUILD == 0 ]]; then
		set -e
		for x in "${OPEN_MP_DLL_LOCS_64_AVX2[@]}"; do 
			cp "$x" lib_manually_provided/win64-avx2/
		done
	fi

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
	
	# Optional avx-2 build
	set +xe
	# Remember in shell land, 0 means true!
	if [[ $USING_AVX2_BUILD == 0 ]]; then
		set -xe
		cp "$VCPKG_ROOT/packages/curl_x64-v3-mingw-dynamic/bin/libcurl.dll" lib_manually_provided/win64-avx2/
		cp "$VCPKG_ROOT/packages/libconfig_x64-v3-mingw-dynamic/bin/libconfig.dll" lib_manually_provided/win64-avx2/
		cp "$VCPKG_ROOT/packages/zlib_x64-v3-mingw-dynamic/bin/libzlib1.dll" lib_manually_provided/win64-avx2/
		cp "$VCPKG_ROOT/packages/openssl_x64-v3-mingw-dynamic/bin/libssl-1_1-x64.dll" lib_manually_provided/win64-avx2/
		cp "$VCPKG_ROOT/packages/openssl_x64-v3-mingw-dynamic/bin/libcrypto-1_1-x64.dll" lib_manually_provided/win64-avx2/
	fi
}

update-windows-shared-libs "$@"


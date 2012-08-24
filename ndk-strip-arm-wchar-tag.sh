#!/bin/bash

# This script "strips" the TAG_ABI_PCS_wchar_t tag from
# libraries in the Android NDK that don't accept wchar_t
# or pass wchar_t to other libraries. Those libraries
# don't have to indicate their wchar_t size; this
# suppresses the GNU linker's wchar_t warning, e.g.:
#
#  ... uses 4-byte wchar_t yet the output is to use
#  2-byte wchar_t; use of wchar_t values across objects
#  may fail
#
# This allows the warning-less use of those libraries
# regardless of wchar_t size, e.g. if you compile your code
# with the -fshort-wchar gcc option.
#
# NOTE: This script "strips" TAG_ABI_PCS_wchar_t from
#       libc.so as well, even though it offers a widechar
#       API. Bionic's widechar API is broken (see comment
#       in <wchar.h>) and I assume you're using the Crystax NDK
#       if you care about libc wchar_t routines behaving
#       correctly.
#
# (Another option is to use the --no-wchar-size-warning
# linker option, but this suppresses the warning completely,
# even when it's important.)

if [ "${NDK_ROOT}" == "" ]; then
	echo Error: NDK_ROOT not set.
	exit 1
fi

if [ ! -f "${NDK_ROOT}/ndk-build" ]; then
	echo Error: ${NDK_ROOT} is not an NDK root directory.
	exit 1
fi

DIR=$(dirname $0)

cd $NDK_ROOT/toolchains

STRIP_ELF=$DIR/strip-elf.sh {}
STRIP_AR=$DIR/strip-ar.sh {}

find . \
	-name '*.so' -or -name '*.o' \
	-path "./arm-linux-*/*" \
	-exec ${STRIP_ELF} \;

find . \
	-name '*.a' \
	-path "./arm-linux-*/*" \
	-exec ${STRIP_AR} \;

cd $NDK_ROOT/platforms

find . \
	\( -name '*.so' -or -name '*.o' \) \
	-not -name 'libcrystax.so' \
	-not -name 'libcrystax_shared.so' \
	-not -name 'libgnustl_shared.so' \
	-not -name 'libstlport_shared.so' \
	-not -name 'libstdc++.so' \
	-not -name 'libgabi++_shared.so' \
	-not -name 'libgnuobjc_shared.so'\
	-path "./android-*/arch-arm/*" \
	-exec ${STRIP_ELF} \;

find . \
	\( -name '*.a' \) \
	-not -name 'libstdc++.a' \
	-path "./android-*/arch-arm/*" \
	-exec ${STRIP_AR} \;
	
cd $NDK_ROOT/sources

find . \
	\( -name '*.a' \) \
	-not -name 'libcrystax_static.a' \
	-not -name 'libgnustl_static.a' \
	-not -name 'libstlport_static.a' \
	-not -name 'libgabi++_static.a' \
	-not -name 'libgnuobjc_static.a' \
	-not -name 'libsupc++.a' \
	-path "*/armeabi*/*" \
	-exec ${STRIP_AR} \;

find . \
	\( -name '*.so' \) \
	-not -name 'libcrystax_shared.so' \
	-not -name 'libgnustl_shared.so' \
	-not -name 'libstlport_shared.so' \
	-not -name 'libgabi++_shared.so' \
	-not -name 'libgnuobjc_shared.so' \
	-not -name 'libsupc++.so' \
	-path "*/armeabi*/*" \
	-exec ${STRIP_ELF} \;

find . \
	\( -name '*.a' \) \
	-not -name 'libcrystax_static.a' \
	-not -name 'libgnustl_static.a' \
	-not -name 'libstlport_static.a' \
	-not -name 'libgabi++_static.a' \
	-not -name 'libgnuobjc_static.a' \
	-not -name 'libsupc++.a' \
	-path "*/armeabi*/*" \
	-exec ${STRIP_AR} \;

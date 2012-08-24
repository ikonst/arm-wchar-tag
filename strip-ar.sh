#!/bin/bash

# This script unpacks an archive (a 'static library'),
# "strips" the TAG_ABI_PCS_wchar_t tag from all its
# object files, and repackages it.

if [ "$1" == "" ]; then
	echo Syntax: strip-ar.sh [file.a]
	exit 1
fi

EABI_WCHAR=$(dirname $0)/arm-eabi-wchar
if [ ! -f $EABI_WCHAR ]; then
	echo arm-eabi-wchar not found.
	exit 1
fi

ARCHIVE=`realpath $1`
if [ ! -f $ARCHIVE ]; then
	echo Archive not found.
	exit 1
fi

TEMPDIR=`mktemp -d`
if [ ! -d "$TEMPDIR" ]; then
	echo Temporary directory not found.
	exit 1
fi

pushd $TEMPDIR > /dev/null || exit 1

echo Unpacking $ARCHIVE to $TEMPDIR...
ar x $ARCHIVE || exit 1

for obj in *.o; do
	echo -n "$obj"
	OUTPUT=`${EABI_WCHAR} $obj 0`
	if [ $? == 0 ]; then
		HAD_SUCCESS=1
	fi
	if [ "$OUTPUT" != "" ];
		then echo ": $OUTPUT";
		else echo;
	fi
done
popd > /dev/null
rm -f $ARCHIVE.new || exit 1

if [ "$HAD_SUCCESS" == "1" ]; then
	ar r $ARCHIVE.new $TEMPDIR/*.o 2>&1 || exit 1
	ranlib $ARCHIVE.new || exit 1
	mv -f $ARCHIVE.new $ARCHIVE
else
	echo $ARCHIVE does not contain readable ELF files. Is it an ARM library?
fi

rm -rf $TEMPDIR || exit 1

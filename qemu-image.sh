#!/bin/bash
#
#   Generate a QEMU flash image from an existing build.
#
#   QEMU boots from a single raw image covering the whole of flash rather than from
#   the separate bootloader, partition table and application binaries used when
#   flashing hardware.  This script merges those binaries into one image using the
#   flash layout recorded by the build, so the offsets never need to be duplicated
#   here.
#
#   A pristine copy of the image is kept alongside it.  QEMU writes to the image in
#   place, so anything the firmware stores in flash (LittleFS contents, NVS and core
#   dumps) survives between runs.  Test runs that need a known starting state restore
#   the pristine copy first.
#
#   Usage: qemu-image.sh <build-directory>
#

set -e
scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

# Check if the shell is interactive.
if [[ $- == *i* ]]; then
  red=`tput setaf 1`
  green=`tput setaf 2`
  reset=`tput sgr0`
fi

BUILD_DIR="$1"

if [ -z "$BUILD_DIR" ]; then
    printf "No build directory supplied.\n"
    printf "Usage: qemu-image.sh <build-directory>\n"
    exit 1
fi

if [ ! -d "$BUILD_DIR" ]; then
    printf "Build directory $BUILD_DIR does not exist.\n"
    printf "Run a build before generating a QEMU image.\n"
    exit 1
fi

BUILD_DIR="$( cd "$BUILD_DIR" ; pwd -P )"

if [ ! -f "$BUILD_DIR/flash_args" ]; then
    printf "$BUILD_DIR/flash_args not found.\n"
    printf "Run a build before generating a QEMU image.\n"
    exit 1
fi

#
#   Default eFuse values for an ESP32 at chip revision 3.  QEMU has no eFuse block of
#   its own so it is given one as a file.  These are the values ESP-IDF itself uses,
#   taken from tools/idf_py_actions/qemu_ext.py.
#
QEMU_EFUSE_HEX="00000000000000000000000000800000000000000000100000000000000000000000000000000000\
00000000000000000000000000000000000000000000000000000000000000000000000000000000\
00000000000000000000000000000000000000000000000000000000000000000000000000000000\
00000000"

#
#   Work out the chip and the flash size from the build rather than assuming them.
#
TARGET=$(python3 -c "import json; print(json.load(open('$BUILD_DIR/project_description.json'))['target'])")
FLASH_SIZE=$(python3 -c "import json; print(json.load(open('$BUILD_DIR/flasher_args.json'))['flash_settings']['flash_size'])")

printf "Generating QEMU image for $TARGET ($FLASH_SIZE flash)..."

cd "$BUILD_DIR"

#
#   Use the esptool bundled with ESP-IDF rather than any standalone esptool that may be
#   on the path.  A separately installed esptool 5 renames the merge_bin command and its
#   flash options, which then disagree with the flash_args file the build writes.  The
#   bundled module accepts the spelling in that file on both a developer machine and in
#   the ESP-IDF container.
#
if command -v python > /dev/null; then
    PYTHON=python
else
    PYTHON=python3
fi

$PYTHON -m esptool --chip "$TARGET" merge_bin \
    --output qemu_flash.bin \
    --fill-flash-size "$FLASH_SIZE" \
    @flash_args > /dev/null

#
#   Keep a pristine copy so a test run can start from a known state, and write the
#   eFuse file if it is not already present.
#
cp qemu_flash.bin qemu_flash_pristine.bin

#
#   Python is used to write the eFuse file rather than xxd, which is not present in the
#   ESP-IDF container.
#
if [ ! -f qemu_efuse.bin ]; then
    $PYTHON -c "import binascii; open('qemu_efuse.bin', 'wb').write(binascii.unhexlify('$QEMU_EFUSE_HEX'))"
fi

printf " ${green}success${reset}\n"
printf "QEMU image written to $BUILD_DIR/qemu_flash.bin\n"

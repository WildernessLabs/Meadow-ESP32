#!/bin/bash
#
#   Boot a QEMU flash image built by qemu-image.sh.
#
#   This is the emulator equivalent of flash.sh: where flash.sh puts the binaries onto
#   real hardware, this runs them under QEMU.  The same image serves the application
#   build and the test build, so the build directory is selected on the command line.
#
#   Three modes are supported:
#
#     default       Serial console attached to this terminal, as idf.py monitor would be.
#     --debug       QEMU starts halted with a GDB stub, ready for a debugger to attach.
#     --serial-tcp  Serial console published on a TCP port for a script to drive.
#
#   Usage: qemu.sh [options]
#

set -e
scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

# Check if the shell is interactive.
if [[ $- == *i* ]]; then
  red=`tput setaf 1`
  green=`tput setaf 2`
  reset=`tput sgr0`
fi

BUILD_DIR="$scriptdir/build"
DEBUG=false
FRESH=false
SERIAL_TCP=false
HELP=false
GDB_PORT=3333
SERIAL_PORT=5555

#
#   Work out which options are on the command line.
#
for i in "$@"
do
case $i in
    -t|--test)
    BUILD_DIR="$scriptdir/test/build"
    ;;
    -b=*|--build-dir=*)
    BUILD_DIR="${i#*=}"
    ;;
    -d|--debug)
    DEBUG=true
    ;;
    -f|--fresh)
    FRESH=true
    ;;
    -s|--serial-tcp)
    SERIAL_TCP=true
    ;;
    --gdb-port=*)
    GDB_PORT="${i#*=}"
    ;;
    --serial-port=*)
    SERIAL_PORT="${i#*=}"
    ;;
    -h|--help)
    HELP=true
    ;;
    *)
    printf "Unknown command line option ($i)."
    printf "Run qemu.sh --help for information on valid options."
    exit 1
    ;;
esac
done

if $HELP; then
    echo "$0: Run a Meadow ESP32 image under QEMU"
    echo ""
    echo "Default action: Boot the application image with the console on this terminal."
    echo ""
    echo "Options:"
    echo "-h | --help             Display this message"
    echo "-t | --test             Run the test build rather than the application build"
    echo "-b=* | --build-dir=*    Run the image in the given build directory"
    echo "-d | --debug            Start halted with a GDB stub and wait for a debugger"
    echo "-f | --fresh            Restore the pristine flash image before booting"
    echo "-s | --serial-tcp       Publish the console on a TCP port instead of this terminal"
    echo "--gdb-port=*            Port for the GDB stub (default 3333)"
    echo "--serial-port=*         Port for the TCP console (default 5555)"
    echo ""
    echo "Press Ctrl-A then X to leave QEMU when the console is on this terminal."
    echo ""
    exit 0
fi

if [ ! -f "$BUILD_DIR/qemu_flash.bin" ]; then
    printf "${red}No QEMU image found in $BUILD_DIR.${reset}\n"
    printf "Build one with build.sh --build --qemu (add --test for the test build).\n"
    exit 1
fi

BUILD_DIR="$( cd "$BUILD_DIR" ; pwd -P )"

if ! command -v qemu-system-xtensa > /dev/null; then
    printf "${red}qemu-system-xtensa was not found on the path.${reset}\n"
    printf "Install it with: python \$IDF_PATH/tools/idf_tools.py install qemu-xtensa\n"
    printf "and make sure the ESP-IDF environment has been exported.\n"
    exit 1
fi

#
#   QEMU writes to the flash image in place, so LittleFS contents, NVS and any core
#   dump survive between runs.  Restore the pristine copy when a known starting state
#   is wanted.
#
if $FRESH; then
    if [ ! -f "$BUILD_DIR/qemu_flash_pristine.bin" ]; then
        printf "${red}No pristine image found in $BUILD_DIR.${reset}\n"
        printf "Regenerate it with build.sh --qemu.\n"
        exit 1
    fi
    cp "$BUILD_DIR/qemu_flash_pristine.bin" "$BUILD_DIR/qemu_flash.bin"
    printf "Restored pristine flash image.\n"
fi

TARGET=$(python3 -c "import json; print(json.load(open('$BUILD_DIR/project_description.json'))['target'])")

if [ "$TARGET" != "esp32" ]; then
    printf "${red}Unsupported target $TARGET.${reset}\n"
    printf "This script currently supports esp32 only.\n"
    exit 1
fi

#
#   Assemble the QEMU command line.  The machine arguments, the eFuse wiring and the
#   watchdog setting match those used by idf.py qemu.  The watchdog is disabled because
#   the interactive test menu busy waits on the console, which would otherwise trip it.
#
QEMU_ARGS=(
    -M esp32
    -m 4M
    -drive "file=$BUILD_DIR/qemu_flash.bin,if=mtd,format=raw"
    -drive "file=$BUILD_DIR/qemu_efuse.bin,if=none,format=raw,id=efuse"
    -global driver=nvram.esp32.efuse,property=drive,value=efuse
    -global driver=timer.esp32.timg,property=wdt_disable,value=true
    -nic user,model=open_eth
    -nographic
)

if $DEBUG; then
    QEMU_ARGS+=(-gdb "tcp::$GDB_PORT" -S)
fi

#
#   The TCP console deliberately waits for a client before the emulator starts.  QEMU
#   discards console output written while nothing is attached, so booting immediately
#   would lose the start of the log, including the prompt the test runner synchronises
#   on, whenever the client is slow to connect.
#
if $SERIAL_TCP; then
    QEMU_ARGS+=(-serial "tcp::$SERIAL_PORT,server")
else
    QEMU_ARGS+=(-serial mon:stdio)
fi

if $DEBUG; then
    printf "${green}QEMU is halted and waiting for a debugger on port $GDB_PORT.${reset}\n"
    printf "Attach with:\n"
    printf "    xtensa-esp32-elf-gdb -x $scriptdir/qemu-gdbinit $BUILD_DIR/MeadowComms.elf\n\n"
fi

if $SERIAL_TCP; then
    printf "${green}Waiting for a console client on TCP port $SERIAL_PORT.${reset}\n\n"
fi

#
#   Replace this shell with QEMU so that whatever started the script, be that a user or
#   the test runner, is holding the emulator's own process and can stop it directly.
#
exec qemu-system-xtensa "${QEMU_ARGS[@]}"

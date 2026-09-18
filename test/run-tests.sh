#!/bin/bash
#
#   Run the unit tests without anyone having to drive the menu by hand.
#
#   The test application presents an interactive menu on its console.  This script
#   hands that menu to SupportScripts/TestRunner.py, which selects the tests, collects
#   the results and reports a pass or a failure.
#
#   Tests can be run under QEMU or on a connected board.  QEMU needs no hardware and
#   suits the file system and storage tests; anything touching WiFi, Bluetooth or the
#   ADC has to run on a board.
#
#   Usage: run-tests.sh [options] [selector ...]
#
#   Selectors are passed to the menu unchanged, for example:
#
#       ./run-tests.sh '[FileSystem]'
#       ./run-tests.sh --hardware '[WiFi]' '[Bluetooth]'
#       ./run-tests.sh '"Delete file"'
#
#   With no selector every registered test is run.
#

set -e
scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

# Check if the shell is interactive.
if [[ $- == *i* ]]; then
  red=`tput setaf 1`
  green=`tput setaf 2`
  reset=`tput sgr0`
fi

HARDWARE=false
DEVICE=""
FRESH=true
VERBOSE=false
HELP=false
TIMEOUT=600
TRANSCRIPT=""
JUNIT=""
SELECTORS=()
EXPECT_REBOOT=()

#
#   Work out which options are on the command line.  Anything that is not a recognised
#   option is taken to be a menu selector.
#
for i in "$@"
do
case $i in
    --hardware)
    HARDWARE=true
    ;;
    -p=*|--port=*)
    DEVICE="${i#*=}"
    HARDWARE=true
    ;;
    --no-fresh)
    FRESH=false
    ;;
    --timeout=*)
    TIMEOUT="${i#*=}"
    ;;
    --transcript=*)
    TRANSCRIPT="${i#*=}"
    ;;
    --junit=*)
    JUNIT="${i#*=}"
    ;;
    --expect-reboot=*)
    EXPECT_REBOOT+=("${i#*=}")
    ;;
    -v|--verbose)
    VERBOSE=true
    ;;
    -h|--help)
    HELP=true
    ;;
    -*)
    printf "Unknown command line option ($i)."
    printf "Run run-tests.sh --help for information on valid options."
    exit 1
    ;;
    *)
    SELECTORS+=("$i")
    ;;
esac
done

if $HELP; then
    echo "$0: Run the Meadow ESP32 unit tests"
    echo ""
    echo "Default action: Run every test under QEMU."
    echo ""
    echo "Options:"
    echo "-h | --help             Display this message"
    echo "--hardware              Run on a connected board rather than under QEMU"
    echo "-p=* | --port=*         Serial device of the board (implies --hardware)"
    echo "--no-fresh              Do not reset to a known state before running"
    echo "--timeout=*             Seconds to allow each selector (default 600)"
    echo "--transcript=*          Write the full console transcript to this file"
    echo "--junit=*               Write the results as a JUnit report to this file"
    echo "--expect-reboot=*       Selector that restarts the device on purpose, so the"
    echo "                        run carries on with the next selector.  Repeatable"
    echo "-v | --verbose          Echo the console output as the tests run"
    echo ""
    echo "Any remaining arguments are passed to the test menu as selectors:"
    echo "    [FileSystem]        every test carrying that tag"
    echo "    *                   every registered test"
    echo '    "Delete file"       a single test, by name'
    echo ""
    echo "Exit status: 0 passed, 1 test failure, 2 timed out, 3 device rebooted."
    echo ""
    exit 0
fi

BUILD_DIR="$scriptdir/build"

if [ ! -d "$BUILD_DIR" ]; then
    printf "${red}No test build found in $BUILD_DIR.${reset}\n"
    printf "Build the tests with build.sh --build --qemu.\n"
    exit 1
fi

RUNNER_ARGS=(--build-dir "$BUILD_DIR" --timeout "$TIMEOUT")

if $HARDWARE; then
    if [ -z "$DEVICE" ]; then
        printf "${red}No serial device given.${reset}\n"
        printf "Supply one with --port=/dev/cu.usbserial-XXXX\n"
        exit 1
    fi
    RUNNER_ARGS+=(--hardware --device "$DEVICE")
else
    if [ ! -f "$BUILD_DIR/qemu_flash.bin" ]; then
        printf "${red}No QEMU image found in $BUILD_DIR.${reset}\n"
        printf "Build one with build.sh --build --qemu.\n"
        exit 1
    fi
    if ! command -v qemu-system-xtensa > /dev/null; then
        printf "${red}qemu-system-xtensa was not found on the path.${reset}\n"
        printf "Install it with: python \$IDF_PATH/tools/idf_tools.py install qemu-xtensa\n"
        exit 1
    fi
fi

if $FRESH; then
    RUNNER_ARGS+=(--fresh)
fi

if $VERBOSE; then
    RUNNER_ARGS+=(--verbose)
fi

if [ ! -z "$TRANSCRIPT" ]; then
    RUNNER_ARGS+=(--transcript "$TRANSCRIPT")
fi

if [ ! -z "$JUNIT" ]; then
    RUNNER_ARGS+=(--junit "$JUNIT")
fi

for selector in "${EXPECT_REBOOT[@]}"; do
    RUNNER_ARGS+=(--expect-reboot "$selector")
done

python3 "$scriptdir/SupportScripts/TestRunner.py" "${RUNNER_ARGS[@]}" "${SELECTORS[@]}"

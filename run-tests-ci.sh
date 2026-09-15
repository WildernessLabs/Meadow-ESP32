#!/bin/bash
#
#   Build and run the emulated unit tests.
#
#   This is the entry point used by the GitHub Actions workflow, and is meant to run
#   inside the espressif/idf container.  It can also be run locally against the same
#   container to reproduce a CI failure, which is the main reason the logic lives here
#   rather than in the workflow file:
#
#       docker run --rm -v $PWD:/project -w /project espressif/idf:release-v5.5 \
#           ./run-tests-ci.sh
#
#   Only the tests that need no radio and no analogue hardware are run.  See
#   Documentation/TestAutomationPlan.md for how the suite is divided.
#

set -e
scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

#
#   [Emulated] selects the tests that are suitable for an unattended run under QEMU.
#   It is a second tag carried alongside the subsystem tag, so a test opts in by being
#   marked rather than by being listed here.
#
#   It excludes tests that need hardware ([System] reads a real ADC, [WiFi] and
#   [Bluetooth] need a radio) and tests that are correct under emulation but too slow
#   to run on every commit, which carry [Slow] instead.
#
SELECTORS=('[Emulated]')

RESULTS_DIR="$scriptdir/test-results"

printf "Installing the QEMU system dependencies...\n"

#
#   The pre-built QEMU binaries are linked against these.  They are not part of the
#   ESP-IDF image, so they have to be added before qemu-system-xtensa will start.
#
apt-get update -qq
apt-get install -y -qq libgcrypt20 libglib2.0-0 libpixman-1-0 libsdl2-2.0-0 libslirp0

printf "Installing QEMU...\n"

python "$IDF_PATH/tools/idf_tools.py" install qemu-xtensa

#
#   idf_tools.py unpacks the binaries but does not put them on the path, so re-export
#   the ESP-IDF environment to pick up qemu-system-xtensa.
#
. "$IDF_PATH/export.sh" > /dev/null

qemu-system-xtensa --version | head -1

#
#   The WiFi tests read their credentials from this file.  It is not in source control
#   and the tests that use it are not run here, but test_wifi.cpp includes it
#   unconditionally so it has to exist for the build to succeed.
#
if [ ! -f "$scriptdir/components/F7Services/test/secrets.h" ]; then
    printf "Creating a placeholder secrets.h...\n"
    cp "$scriptdir/test/main/secrets.template" \
       "$scriptdir/components/F7Services/test/secrets.h"
fi

#
#   The test build needs build_info.h and the build/config directory, both of which are
#   produced by the application build.  See Documentation/Testing.md.
#
printf "Building the application...\n"
"$scriptdir/build.sh" --build

printf "Building the tests and the QEMU image...\n"
"$scriptdir/test/build.sh" --build --qemu

printf "Running the tests...\n"
mkdir -p "$RESULTS_DIR"

#
#   run-tests.sh exits non-zero when a test fails, times out or the device reboots.
#   Capture that rather than letting set -e end the script, so the reports are always
#   written and can be published by the workflow.
#
set +e
"$scriptdir/test/run-tests.sh" \
    --junit="$RESULTS_DIR/qemu-tests.xml" \
    --transcript="$RESULTS_DIR/qemu-console.log" \
    "${SELECTORS[@]}"
RESULT=$?
set -e

case $RESULT in
    0) printf "\nAll tests passed.\n" ;;
    1) printf "\nA test failed.\n" ;;
    2) printf "\nA selector timed out.\n" ;;
    3) printf "\nThe device rebooted during a test.\n" ;;
    *) printf "\nThe test run failed with status %s.\n" "$RESULT" ;;
esac

exit $RESULT

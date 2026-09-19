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

#
#   The configuration tests run under QEMU but are not tagged [Emulated].  Answering a
#   System message builds a SystemRequestHandler, and its constructor mounts the file
#   system, so running them inside the [Emulated] batch would leave a partition mounted
#   underneath the [FileSystem] tests.  Building that handler also claims a singleton for
#   the rest of the boot.
#
#   They therefore run as a selector of their own, after [Emulated] and before the core
#   dump tests, the first of which restarts the device and so hands the singletons back.
#
SELECTORS+=('[Configuration]')

#
#   The core dump tests are not tagged [Emulated] and are named individually, because
#   they have to run in this order and because the first of them restarts the device.
#   QEMU saves a core dump to flash and reads it back exactly as the hardware does, so
#   they need no board, but the menu runs a tag as one batch and a restart part way
#   through loses its place.  Naming the crashing test on its own, and telling the runner
#   with --expect-reboot that the restart is deliberate, keeps them in the same session
#   and so against the same flash as the core dump they read.
#
#   They run after [Emulated] so that a restart cannot disturb it, and the last of them
#   erases the core dump again.
#
#   The whole sequence, including the full retrieval, has been run under QEMU on a
#   developer machine and takes a couple of minutes.  If it ever gives trouble on the CI
#   runner, delete SELECTORS from '"Generate a core dump"' onwards and the
#   --expect-reboot line below; the [Emulated] run is unaffected by their removal.
#
SELECTORS+=(
    '"Generate a core dump"'
    '"Get core dump information"'
    '"Get core dump fragment"'
    '"Retrieve the whole core dump"'
    '"Erase core dump"'
)
EXPECT_REBOOT='--expect-reboot="Generate a core dump"'

#
#   The MessageDispatcher tests run under QEMU but are not tagged [Emulated] either.
#   Building a dispatcher creates the WiFi, Bluetooth and System request handlers, which
#   are singletons, so it claims them for the rest of the boot and any later test that
#   wanted one of its own would reset the board.  Running them as a selector of their own,
#   last, keeps that to themselves.
#
SELECTORS+=('[MessageDispatcher]')

#
#   The WiFi configuration tests follow the MessageDispatcher tests and reuse the
#   WiFiRequestHandler those built.  The handler is a singleton and a second instance
#   resets the board, so the order of these two selectors matters.  They need no radio:
#   the country code is read from NVS by the constructor and nothing here starts the
#   interface.
#
SELECTORS+=('[WiFiConfiguration]')

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
#   Nothing is needed here to let the build succeed.  The settings the tests use are read
#   from a file on the device at run time, and the emulated tests do not need any of them:
#   the run reports that no configuration was found and carries on with the defaults.
#

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
    "$EXPECT_REBOOT" \
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

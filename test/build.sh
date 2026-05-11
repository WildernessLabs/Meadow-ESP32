#!/bin/bash

set -e
scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

#
#   Work out the OS so that we can change actions per OS where necessary.
#
shopt -s nocasematch
case "$(uname -a)" in
  *darwin*)
    OS="mac"
    ;;
  *linux*)
    OS="linux"
    ;;
  cygwin*|mingw32*|msys*|mingw*)
    OS="windows"
    ;;
  *)
    OS="unknown"
    ;;
esac

# Check if the shell is interactive.
if [[ $- == *i* ]]; then
  red=`tput setaf 1`
  green=`tput setaf 2`
  reset=`tput sgr0`
fi

VERBOSE=true
CLEAN=false
FULLCLEAN=false
COREDUMP=false
MENUCONFIG=false
HELP=false
DRYRUN=false
HEAPTRACING=false
IDF_FLAGS=""
IDF_DEFINES=""
DEBUG_SELECTIONS=""
TEST_SELECTIONS=""
FLASH=false

#
#   Work out which options are on the command line.
#
for i in "$@"
do
case $i in
    -v|--verbose)
    VERBOSE=true
    ;;
    -b|--build)
    IDF_FLAGS+=" build"
    ;;
    -c|--clean)
    IDF_FLAGS+=" clean"
    ;;
    --fullclean)
    IDF_FLAGS+=" fullclean"
    ;;
    --flash)
    FLASH=true
    ;;
    -tm|--tracemessages)
    IDF_DEFINES+=" -DTRACE_MESSAGES=1"
    ;;
    --MENUCONFIG)
    MENUCONFIG=true
    ;;
    --dryrun)
    DRYRUN=true
    ;;
    -f|--flash)
    # IDF_FLAGS+=" flash"
    FLASH=true
    ;;
    -m|--monitor)
    IDF_FLAGS+=" monitor"
    ;;
    -d=*|--debug=*)
    DEBUG_SELECTIONS="${i#*=}"
    ;;
    --halt)
    IDF_DEFINES+=" -DCONFIG_ESP_SYSTEM_PANIC_PRINT_HALT=y"
    ;;
    # --tests=*)
    # TEST_SELECTIONS="${i#*=}"
    # ;;
    -h|--help)
    HELP=true
    ;;
    *)
    printf "Unknown command line option ($i)."
    printf "Run build.sh --help for information on valid options."
    exit 1
    ;;
esac
done

if [[ $# -eq 0 ]]; then
    # No arguments were passed
    echo "No arguments supplied, performing default build."
    BUILD=true
    IDF_FLAGS+=" build"
fi

if $HELP; then
    echo "$0: Build the ESP code for Meadow"
    echo ""
    echo "Default action: Build the ESP application in release mode."
    echo ""
    echo "Options:"
    echo "-h | --help             Display this message"
    echo "-c | --clean            Perform a clean build"
    echo "-f | --flash            Flash the ESP32 with the bootloader, partition table and application code"
    echo "-m | --monitor          Connect to the serial port for application monitoring"
    echo "--fullclean             Perform a full clean followed by a build"
    echo "-tm | --tracemessages   Enable heap tracing."
    echo "--menuconfig            Enter the menu configuration system overwriting sdkconfig.defaults"
    echo "--halt                  Halt the system on hard fault."
    echo "-d=* | --debug=*        Turn on debug for selected system(s). Valid values (comma separated): WIFI, SPI, BT, SYSTEM."
    echo ""
    exit 0
fi

run_command() {
  if $VERBOSE || $DRYRUN; then
    echo "Executing command: $1"
    if ! $DRYRUN; then
        $1
    fi
  else
    if ! $DRYRUN; then
        $1 &>/dev/null
    fi
  fi
}

check_command_status() {
  exit_status=$?
  if [ $exit_status -ne 0 ]; then
    printf " ${red}error${reset}\n"
    if ! $VERBOSE; then
        printf "Re-run the script with --verbose flag to see the output.\n"
    fi
    exit 1
  else
    printf " ${green}success${reset}\n"
  fi
}

#
#   Enter the configuration system for the application.  This takes care
#   of the fact that we have a default configuration system and makes sure
#   we edit the release configuration file.
#
if $MENUCONFIG; then
    SDKCONFIG="sdkconfig"
    DEFAULT_SDKCONFIG="sdkconfig.defaults"
    if test -f "$SDKCONFIG"; then
        run_command "rm $SDKCONFIG"
    fi
    run_command "mv $DEFAULT_SDKCONFIG $SDKCONFIG"
    run_command "idf.py menuconfig"
    run_command "mv $SDKCONFIG $DEFAULT_SDKCONFIG"
    exit 0
fi

#
#   Work out if we need to add any defintions to enable logging.
#
if [ ! -z "$DEBUG_SELECTIONS" ]; then
    subsystems=$(echo $DEBUG_SELECTIONS | tr "," "\n")
    for subsystem in $subsystems
    do
        case $subsystem in
            wifi)
            IDF_DEFINES+=" -DWIFI_DEBUG=1"
            ;;
            spi)
            IDF_DEFINES+=" -DSPI_DEBUG=1"
            ;;
            system)
            IDF_DEFINES+=" -DSYSTEM_DEBUG=1 -DFILE_SYSTEM_DEBUG=1"
            ;;
            messages)
            IDF_DEFINES+=" -DMESSAGES_DEBUG=1"
            ;;
            bt)
            IDF_DEFINES+=" -DBLUETOOTH_DEBUG=1"
            ;;
            thread)
            IDF_DEFINES+=" -DTHREAD_DEBUG=1"
            ;;
            *)
            printf "Unknown debug subsystem $subsystem.\n"
            printf "Run build.sh --help for information on valid options.\n"
            exit 1
            ;;
        esac
    done
fi

#
#   There are sometimes problems with old configuration in the build directory
#   so this removes the build directory before executing the full clean.
#
if $FULLCLEAN; then
    run_command "rm -Rf $scriptdir/build"
    run_command "idf.py fullclean"
    run_command "rm -f $scriptdir/sdkconfig"
fi

if ! $NO_BUILD; then
    IDF_FLAGS="build $IDF_FLAGS"
fi
if [[ ! -z $IDF_FLAGS ]]; then
    run_command "idf.py $IDF_DEFINES $IDF_FLAGS"
fi

if [ $? -eq 0 ]; then
  if $FLASH; then
    $scriptdir/flash.sh
  fi
fi

now=$(date +"%T")
printf "Build of version $VERSION_MAJOR.$VERSION_MINOR.$VERSION_REVISION.$VERSION_BUILD (${BUILD_GIT_HASH:0-8}:$BUILD_GIT_REF) finished at $now\n"

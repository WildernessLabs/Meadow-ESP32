#!/bin/bash

#set -e
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

VERBOSE=false
CLEAN=false
FULLCLEAN=false
COREDUMP=false
HEAP=false
DEBUG=false
MENUCONFIG=false
HELP=false
DRYRUN=false
NO_BUILD=false
IDF_FLAGS=""
IDF_DEFINES=""

#
#   Work out which options are on the command line.
#
for i in "$@"
do
case $i in
    -v|--verbose)
    VERBOSE=true
    ;;
    -c|--clean)
    IDF_FLAGS+=" clean"
    ;;
    --fullclean)
    FULLCLEAN=true
    ;;
    -d|--coredump)
    IDF_DEFINES+=" -DCOREDUMP=1"
    ;;
    --MENUCONFIG)
    MENUCONFIG=true
    ;;    
    --debug)
    DEBUG=true
    ;;
    --dryrun)
    DRYRUN=true
    ;;
    -f|--flash)
    IDF_FLAGS+=" flash"
    ;;
    -m|--monitor)
    IDF_FLAGS+=" monitor"
    ;;
    -n|--nobuild)
    NO_BUILD=true
    ;;
    -h|--help)
    HELP=true
    ;;
    *)
    # unknown option
    ;;
esac
done

if $HELP; then
    echo "$0: Build the ESP code for Meadow"
    echo ""
    echo "Default action: Build the ESP application in release mode."
    echo ""
    echo "Options:"
    echo "-h | --help          Display this message"
    echo "-n | --nobuild       Do not build the system"
    # echo "--debug              Compile with debugging turned on"
    echo "-c | --clean         Perform a clean build"
    echo "--fullclean          Perform a full clean followed by a build"
    echo "-v | --verbose       Display commands as they are executed"
    echo "-d | --coredump      Add core dump functionality to the application"
    echo "-m | --menuconfig    Enter the menu configuration system overwriting sdkconfig.defaults"
    echo "--dryrun             Perform a dry run (do not execute commands)"
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
#   There are sometimes some problems with old configuration in the build directory
#   so this removes the build directory before executing the full clean.
#
if $FULLCLEAN; then
    run_command "rm -R build"
    run_command "idf.py fullclean"
fi

if $DEBUG; then
    echo "--debug is not implemented"
    exit 0
fi

if $HEAP; then
    echo "Heap tracing functionality is not integrated into this script at the moment."
    exit 0
fi

#
#   If we get here then we have stuff to do so fire up the build system.
#
if ! $NO_BUILD; then
    IDF_FLAGS="build $IDF_FLAGS"
fi
if [[ ! -z $IDF_FLAGS ]]; then
    run_command "idf.py $IDF_DEFINES $IDF_FLAGS"
    echo "Build finished!"
else
    echo "Nothing more to do."
fi
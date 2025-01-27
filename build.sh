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
GENERATE_SBOM=false

#
#   Work out which options are on the command line.
#
for i in "$@"
do
case $i in
    -v|--verbose)
    VERBOSE=true
    ;;
    -s|--silent)
    VERBOSE=false
    ;;
    -b|--build)
    IDF_FLAGS+=" build"
    ;;
    -c|--clean)
    IDF_FLAGS+=" clean"
    ;;
    --fullclean)
    FULLCLEAN=true
    ;;
    --flash)
    FLASH=true
    ;;
    --coredump)
    IDF_DEFINES+=" -DCOREDUMP=1"
    ;;
    -th|--heaptracing)
    IDF_DEFINES+=" -DHEAPTRACING=1"
    ;;
    -tm|--tracemessages)
    IDF_DEFINES+=" -DTRACE_MESSAGES=1"
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
    --sbom)
    GENERATE_SBOM=true
    ;;
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
    echo "-v | --verbose          Display commands as they are executed (default)"
    echo "-s | --silent           Execute commands silently"
    echo "--coredump              Add core dump functionality to the application"
    echo "-th | --heaptracing     Enable heap tracing."
    echo "-tm | --tracemessages   Enable heap tracing."
    echo "--menuconfig            Enter the menu configuration system overwriting sdkconfig.defaults"
    echo "--dryrun                Perform a dry run (do not execute commands)"
    echo "--halt                  Halt the system on hard fault."
    echo "--sbom                  Generate a Software Bill of Materials (SBOM) in SPDX format."
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

get_git_commit_hash() {
  REPO_PATH=$1
  echo `git -C $REPO_PATH rev-parse HEAD`
}

get_git_branch_or_tag() {
  REPO_PATH=$1
  echo `git -C $REPO_PATH describe --tags --exact-match 2> /dev/null || git -C $REPO_PATH symbolic-ref -q --short HEAD`
}

get_version_change_distance() {
  REPO_PATH=$1
  origin=$(git log -n 1 --oneline $scriptdir/version.txt  | cut -f 1 -d " ")
  distance=$(git log --oneline ${origin}..HEAD | wc -l)
  echo $distance
}

sedFriendly() {
  result=$(echo $1 | sed -r 's/([\$\.\*\/\[\\^])/\\\1/g'|sed 's/[]]/\[]]/g')
  echo $result
}

inject_value() {
  KEY=$1
  VALUE=$(eval echo '${'$KEY'}')
  REPLACEMENT=$(sedFriendly $VALUE)
  FILE=$2
  echo "Replacing ###${KEY}### with ${VALUE} in $FILE"
  sed -i.bak 's/###'${KEY}'###/'${REPLACEMENT}'/g' $FILE
}

generate_build_info() {
  printf "Generating build info..."

  if [ "$scriptdir" == "/project" ]; then
    git config --global --add safe.directory /project
  fi

  BUILD_GIT_HASH=$(get_git_commit_hash .)
  BUILD_GIT_REF=$(get_git_branch_or_tag .)

  git checkout HEAD $scriptdir/version.txt

  read -r BUILD_VERSION_STRING<$scriptdir/version.txt || true
  IFS='.' read -ra BUILD_VERSION <<< "$BUILD_VERSION_STRING"
  VERSION_MAJOR=${BUILD_VERSION[0]}
  VERSION_MINOR=${BUILD_VERSION[1]}
  VERSION_REVISION=${BUILD_VERSION[2]}

  VERSION_BUILD=$(get_version_change_distance $1)

  echo "$VERSION_MAJOR.$VERSION_MINOR.$VERSION_REVISION.$VERSION_BUILD" > $scriptdir/version.txt

  echo Calculated version: ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_REVISION}.${VERSION_BUILD} '('${BUILD_GIT_HASH:0-8}:${BUILD_GIT_REF}')'

  git checkout HEAD $scriptdir/main/version.h

  #
  # Get the date / time components in UTC format.
  #
  # These macros may look odd but the date formatting can return date components in the form
  # 00, 01, 02 etc and these when compiled are taken as octal numbers.  This means 09 is an
  # invalid number for the compiler so it it is necessary to remove the leading 0 and put it
  # back when formatting the date/time output for the user.
  #
  BUILD_DAY=$((10#`date -u +"%d"`))
  BUILD_TWO_DIGIT_DAY=`date -u +"%d"`
  BUILD_MONTH=$((10#`date -u +"%m"`))
  BUILD_TWO_DIGIT_MONTH=`date -u +"%m"`
  BUILD_MONTH_NAME=`date -u +"%b"`
  BUILD_YEAR=$((10#`date -u +"%y"`))
  BUILD_HOUR=$((10#`date -u +"%H"`))
  BUILD_TWO_DIGIT_HOUR=`date -u +"%H"`
  BUILD_MINUTE=$((10#`date -u +"%M"`))
  BUILD_TWO_DIGIT_MINUTE=`date -u +"%M"`
  BUILD_SECOND=$((10#`date -u +"%S"`))
  BUILD_TWO_DIGIT_SECOND=`date -u +"%S"`
  BUILD_HASH="0x${BUILD_GIT_HASH:0-8}"

  for s in $(echo VERSION_MAJOR VERSION_MINOR VERSION_REVISION VERSION_BUILD BUILD_DAY BUILD_TWO_DIGIT_DAY BUILD_MONTH BUILD_TWO_DIGIT_MONTH BUILD_MONTH_NAME BUILD_YEAR BUILD_HOUR BUILD_TWO_DIGIT_HOUR HOUR BUILD_MINUTE BUILD_TWO_DIGIT_MINUTE BUILD_SECOND BUILD_TWO_DIGIT_SECOND BUILD_HASH BUILD_GIT_REF)
  do
    inject_value $s $scriptdir/main/build_info.template
  done
   cp $scriptdir/main/build_info.template $scriptdir/main/build_info.h

  printf " ${green}success${reset}\n"
}

generate_build_info

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
#   The ESP unit tests require a secrets file to be present so check if there is one
#   available and copy it to the right place if it is available.  This file does not
#   want to find its way its way into source control so its existence will be checked
#   later and it will be removed (assuming success).
#
if test -f "$scriptdir/../secrets.h"; then
    cp $scriptdir/../secrets.h $scriptdir/test/secrets.h
    cp $scriptdir/../secrets.h $scriptdir/components/F7Services/test/secrets.h
fi

#
#   There are sometimes problems with old configuration in the build directory
#   so this removes the build directory before executing the full clean.
#
if $FULLCLEAN; then
    run_command "rm -Rf $scriptdir/build"
    run_command "idf.py --project-dir $scriptdir fullclean"
fi

#
#   If we get here then we have stuff to do so fire up the build system.
#
#   First thing, delete the object file containing the version information so
#   we always get an updated version number and build time.
#
find . -name "AppMain.*.obj" -exec rm {} \;

if ! $NO_BUILD; then
    IDF_FLAGS="build $IDF_FLAGS"
fi
if [[ ! -z $IDF_FLAGS ]]; then
    idf.py -C $scriptdir $IDF_DEFINES $IDF_FLAGS
fi

if [ $? -eq 0 ]; then
  if $FLASH; then
    $scriptdir/flash.sh
  fi
fi

#
# Generate the SBOM (Software Bill of Materials) in SPDX format and them check the SBOM and generate a report file.
#
if $GENERATE_SBOM; then
  echo "Generating SBOM..."
  esp-idf-sbom create --rem-unused --rem-config -o $scriptdir/sbom/MeadowComms.spdx $scriptdir/build/project_description.json
  esp-idf-sbom check $scriptdir/sbom/MeadowComms.spdx -o $scriptdir/sbom/MeadowComms.txt
fi

#Generate checksums for binaries

md5sum  $scriptdir/build/MeadowComms.bin | cut -d ' ' -f 1 > $scriptdir/build/MeadowComms.bin.md5
md5sum  $scriptdir/build/bootloader/bootloader.bin | cut -d ' ' -f 1 > $scriptdir/build/bootloader/bootloader.bin.md5
md5sum  $scriptdir/build/partition_table/partition-table.bin | cut -d ' ' -f 1 > $scriptdir/build/partition_table/partition-table.bin.md5

git checkout HEAD $scriptdir/main/build_info.template
git checkout HEAD $scriptdir/version.txt
rm $scriptdir/main/build_info.template.bak

now=$(date +"%T")
printf "Build of version $VERSION_MAJOR.$VERSION_MINOR.$VERSION_REVISION.$VERSION_BUILD (${BUILD_GIT_HASH:0-8}:$BUILD_GIT_REF) finished at $now\n"

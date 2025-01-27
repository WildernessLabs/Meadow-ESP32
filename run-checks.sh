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

VERBOSE_FLAGS=
HELP=false

#
#   Work out which options are on the command line.
#
for i in "$@"
do
case $i in
    -v|--verbose)
    VERBOSE_FLAGS=--verbose
    ;;
    -h|--help)
    HELP=true
    ;;
    *)
    printf "Uknown command line option ($i)."
    printf "Run run-checks.sh --help for information on valid options."
    exit 1
    ;;
esac
done

if $HELP; then
    echo "$0: Run code checks."
    echo ""
    echo "Options:"
    echo "-h | --help             Display this message"
    echo "-v | --verbose          Execute in verbose mode (useful for debugging)"
    echo ""
    exit 0
fi


SOURCE_DIR=$scriptdir
COMPONENTS_DIR=$SOURCE_DIR/components/F7Services

echo "Running cppcheck"
cppcheck --error-exitcode=1 --quiet --check-level=exhaustive --force --inline-suppr -iCMakeLists.txt -icomponents/esp_littlefs -ibuild .

echo "Running clang-format"
clang-format --dry-run --Werror $VERBOSE_FLAGS $SOURCE_DIR/main/*.?pp
clang-format --dry-run --Werror $VERBOSE_FLAGS $COMPONENTS_DIR/*.?pp
clang-format --dry-run --Werror $VERBOSE_FLAGS $COMPONENTS_DIR/Bluetooth/*.?pp
clang-format --dry-run --Werror $VERBOSE_FLAGS $COMPONENTS_DIR/Exceptions/*.?pp
clang-format --dry-run --Werror $VERBOSE_FLAGS $COMPONENTS_DIR/MeshNetwork/*.?pp
clang-format --dry-run --Werror $VERBOSE_FLAGS $COMPONENTS_DIR/Messaging/*.?pp
clang-format --dry-run --Werror $VERBOSE_FLAGS $COMPONENTS_DIR/System/*.?pp
clang-format --dry-run --Werror $VERBOSE_FLAGS $COMPONENTS_DIR/test/*.?pp
clang-format --dry-run --Werror $VERBOSE_FLAGS $COMPONENTS_DIR/WiFi/*.?pp

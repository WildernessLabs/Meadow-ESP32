#!/bin/bash

scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

# Check if the shell is interactive.
if [[ $- == *i* ]]; then
  red=`tput setaf 1`
  green=`tput setaf 2`
  reset=`tput sgr0`
  bold=`tput bold`
fi

VERBOSE=true
FORCE=false
ESP=false

for i in "$@"
do
case $i in
    -v|--verbose)
    VERBOSE=true
    ;;
    -f|--force)
    FORCE=true
    ;;
    *)
    # unknown option
    ;;
esac
done

run_command() {
  if $VERBOSE; then
    echo
    $1
  else
    $1 &>/dev/null
  fi
}

check_command_status() {
  exit_status=$?
  if [ $exit_status -ne 0 ]; then
    printf " ${red}error${reset}\n"
    if ! $VERBOSE; then
      if [ "$DFU" = true ] ; then
        printf "Try --ocd mode or --verbose flag to see the output.\n"
      else
        printf "Try --dfu mode or --verbose flag to see the output.\n"
      fi
    fi
    exit 1
  else
    printf " ${green}success${reset}\n"
  fi
}

#
#   Flash the board with openocd
#

OCD=true
printf "Flashing ESP32 binaries using OpenOCD... "

run_command "$IDF_TOOLS_PATH/tools/openocd-esp32/v0.10.0-esp32-20191114/openocd-esp32/bin/openocd -s share/openocd/scripts -f $scriptdir/flash.gdbinit"
check_command_status

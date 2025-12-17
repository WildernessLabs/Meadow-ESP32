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

#
#   Parse command line arguments
#
MODE="info"
FILENAME=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    -i)
      MODE="info"
      shift
      ;;
    -d)
      MODE="debug"
      shift
      ;;
    *)
      FILENAME="$1"
      shift
      ;;
  esac
done

#
#   Validate filename parameter
#
if [[ -z "$FILENAME" ]]; then
  echo "${red}Error: Filename parameter is required${reset}"
  echo "Usage: $0 [-i|-d] <filename>"
  echo "  -i : Display coredump information (default)"
  echo "  -d : Start debugger with coredump"
  exit 1
fi

#
#   Check if file exists
#
if [[ ! -f "$FILENAME" ]]; then
  echo "${red}Error: File '$FILENAME' does not exist${reset}"
  exit 1
fi

#
#   Execute appropriate command
#
if [[ "$MODE" == "debug" ]]; then
  echo "${green}Starting debugger with coredump: $FILENAME${reset}"
  idf.py coredump-debug -c "$FILENAME"
else
  echo "${green}Displaying coredump information: $FILENAME${reset}"
  idf.py coredump-info -c "$FILENAME"
fi

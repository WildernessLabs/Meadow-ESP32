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

VERBOSE=false
DRYRUN=false

# Check if the shell is interactive.
if [[ $- == *i* ]]; then
  red=`tput setaf 1`
  green=`tput setaf 2`
  reset=`tput sgr0`
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

#
# Generate the SBOM (Software Bill of Materials) in SPDX format and them check the SBOM and generate a report file.
#
echo "Generating SBOM..."
run_command "esp-idf-sbom create --rem-unused --rem-config -o $scriptdir/sbom/MeadowComms.spdx $scriptdir/build/project_description.json"
run_command "esp-idf-sbom check $scriptdir/sbom/MeadowComms.spdx -o $scriptdir/sbom/MeadowComms.txt"


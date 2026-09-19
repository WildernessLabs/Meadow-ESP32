#!/bin/bash
#
#   Build a LittleFS image from a directory on this machine and, if asked, write it to
#   the board.
#
#   The image is not built by build.sh and is not written by flash.sh.  It holds
#   configuration rather than code, changes rarely, and writing it costs a second of
#   flash wear, so it is asked for rather than assumed.  The tests normally need one;
#   the application can use one and does not have to.
#
#   Every value that describes the image has to match what the firmware expects or the
#   mount fails, and a failed mount is not reported: FileSystem sets
#   format_if_mount_failed, so the partition is quietly replaced with an empty one and
#   the only sign is the file having gone.  The values are therefore read from the
#   project rather than written down here.
#

set -e
scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

# Check if the shell is interactive.
if [[ $- == *i* ]]; then
  red=`tput setaf 1`
  green=`tput setaf 2`
  reset=`tput sgr0`
  bold=`tput bold`
fi

PROJECT=test
SOURCE=
OUTPUT=
BLOCK_SIZE=4096
FLASH=false
REBUILD_TOOL=false
VERBOSE=false
HELP=false

for i in "$@"
do
case $i in
    -p=*|--project=*)
    PROJECT="${i#*=}"
    ;;
    -s=*|--source=*)
    SOURCE="${i#*=}"
    ;;
    -o=*|--output=*)
    OUTPUT="${i#*=}"
    ;;
    --block-size=*)
    BLOCK_SIZE="${i#*=}"
    ;;
    -f|--flash)
    FLASH=true
    ;;
    --rebuild-tool)
    REBUILD_TOOL=true
    ;;
    -v|--verbose)
    VERBOSE=true
    ;;
    -h|--help)
    HELP=true
    ;;
    *)
    printf "Unknown command line option ($i).\n"
    printf "Run mkfsimage.sh --help for information on valid options.\n"
    exit 1
    ;;
esac
done

if $HELP; then
    echo "$0: Build a LittleFS image and optionally write it to the board."
    echo ""
    echo "The image is built from a directory of files on this machine.  Its geometry is"
    echo "read from the project's partitions.csv and sdkconfig, so it cannot drift from"
    echo "what the firmware will try to mount."
    echo ""
    echo "Options:"
    echo "-h | --help             Display this message"
    echo "-p=* | --project=*      Project supplying the geometry (default: test)"
    echo "                        Use --project=. for the application"
    echo "-s=* | --source=*       Directory holding the files (default: <project>/fsimage)"
    echo "-o=* | --output=*       Image to write (default: <project>/littlefs.bin)"
    echo "--block-size=*          LittleFS block size (default: 4096, the flash sector)"
    echo "-f | --flash            Write the image to the board once it has been built"
    echo "--rebuild-tool          Build mklittlefs again even if it is already there"
    echo "-v | --verbose          Echo the commands as they run"
    echo ""
    echo "Examples:"
    echo "    ./mkfsimage.sh                       build the test image"
    echo "    ./mkfsimage.sh --flash               build it and write it to the board"
    echo "    ./mkfsimage.sh --project=. --flash   the same for the application"
    echo ""
    exit 0
fi

PROJECT_DIR="$( cd "$scriptdir/$PROJECT" 2>/dev/null && pwd -P )" || {
    printf "${red}No such project directory: $scriptdir/$PROJECT${reset}\n"
    exit 1
}

[ -z "$SOURCE" ] && SOURCE="$PROJECT_DIR/fsimage"
[ -z "$OUTPUT" ] && OUTPUT="$PROJECT_DIR/littlefs.bin"

PARTITIONS="$PROJECT_DIR/partitions.csv"
SDKCONFIG="$PROJECT_DIR/sdkconfig"
MKLITTLEFS_DIR="$PROJECT_DIR/managed_components/joltwallet__littlefs/mklittlefs"
MKLITTLEFS="$MKLITTLEFS_DIR/mklittlefs"

for required in "$PARTITIONS" "$SDKCONFIG" "$MKLITTLEFS_DIR"; do
    if [ ! -e "$required" ]; then
        printf "${red}Cannot find $required${reset}\n"
        printf "The project has to have been configured at least once before an image can be built.\n"
        exit 1
    fi
done

if [ ! -d "$SOURCE" ]; then
    printf "${red}No such source directory: $SOURCE${reset}\n"
    exit 1
fi

#
#   Read the geometry from the project.  The offset and the size come from the partition
#   table, the page size and the longest name the firmware will accept come from the
#   configuration it was built with.
#
PARTITION_LINE=$(grep -E "^\s*littlefs\s*," "$PARTITIONS" || true)
if [ -z "$PARTITION_LINE" ]; then
    printf "${red}No littlefs partition in $PARTITIONS${reset}\n"
    exit 1
fi

OFFSET=$(echo "$PARTITION_LINE" | awk -F, '{gsub(/ /,"",$4); print $4}')
SIZE=$(echo "$PARTITION_LINE" | awk -F, '{gsub(/ /,"",$5); print $5}')
PAGE_SIZE=$(grep -E "^CONFIG_LITTLEFS_PAGE_SIZE=" "$SDKCONFIG" | cut -d= -f2)
NAME_MAX=$(grep -E "^CONFIG_LITTLEFS_OBJ_NAME_LEN=" "$SDKCONFIG" | cut -d= -f2)

if [ -z "$OFFSET" ] || [ -z "$SIZE" ] || [ -z "$PAGE_SIZE" ] || [ -z "$NAME_MAX" ]; then
    printf "${red}Could not read the image geometry from the project.${reset}\n"
    printf "  offset=%s size=%s page=%s name_max=%s\n" "$OFFSET" "$SIZE" "$PAGE_SIZE" "$NAME_MAX"
    exit 1
fi

printf "${bold}Image geometry, taken from $PROJECT${reset}\n"
printf "  partition offset : %s\n" "$OFFSET"
printf "  partition size   : %s\n" "$SIZE"
printf "  page size        : %s\n" "$PAGE_SIZE"
printf "  block size       : %s\n" "$BLOCK_SIZE"
printf "  longest name     : %s\n" "$NAME_MAX"
printf "  source directory : %s\n" "$SOURCE"
printf "  image            : %s\n" "$OUTPUT"
printf "\n"

#
#   mklittlefs writes the longest name it was built for into the superblock, and the
#   firmware refuses an image whose limit is longer than its own.  A stock build uses 32,
#   which mounts but silently shortens longer names, so the tool is built here with the
#   value the firmware was configured with and rebuilt if that value changes.
#
STAMP="$MKLITTLEFS_DIR/.name_max"
if [ ! -x "$MKLITTLEFS" ] || [ ! -f "$STAMP" ] || [ "$(cat "$STAMP")" != "$NAME_MAX" ]; then
    REBUILD_TOOL=true
fi

if $REBUILD_TOOL; then
    #
    #   The compilers are named explicitly.  Inside a shell that has sourced the ESP-IDF
    #   environment, clang++ is the cross compiler for the target, and building a tool
    #   that has to run on this machine with it fails somewhere unhelpful.
    #
    printf "Building mklittlefs for this machine, longest name %s... " "$NAME_MAX"
    if $VERBOSE; then
        (cd "$MKLITTLEFS_DIR" && make clean && make CC=/usr/bin/clang CXX=/usr/bin/clang++ STRIP=/usr/bin/strip LFS_NAME_MAX="$NAME_MAX")
    else
        (cd "$MKLITTLEFS_DIR" && make clean && make CC=/usr/bin/clang CXX=/usr/bin/clang++ STRIP=/usr/bin/strip LFS_NAME_MAX="$NAME_MAX") > /dev/null 2>&1
    fi
    echo "$NAME_MAX" > "$STAMP"
    printf "${green}success${reset}\n"
fi

#
#   The files are staged rather than the directory being handed over as it stands.  It
#   holds things that belong to this machine and not to the board: .gitkeep, which only
#   exists so the directory survives a clone, and the templates the real files are copied
#   from.  Putting either on the device wastes space and invites someone to read a
#   template believing it to be configuration.
#
STAGING=$(mktemp -d -t mkfsimage)
trap 'rm -rf "$STAGING"' EXIT

find "$SOURCE" -type f ! -name '.*' ! -name '*.template' -exec cp {} "$STAGING/" \;

STAGED_COUNT=$(find "$STAGING" -type f | wc -l | tr -d ' ')
if [ "$STAGED_COUNT" -eq 0 ]; then
    printf "${red}No files to put in the image.${reset}\n"
    printf "$SOURCE holds nothing that is not a dot file or a template.\n"
    exit 1
fi

printf "Building the image from $STAGED_COUNT file(s)... "
if $VERBOSE; then
    "$MKLITTLEFS" -c "$STAGING" -s "$SIZE" -p "$PAGE_SIZE" -b "$BLOCK_SIZE" "$OUTPUT"
else
    "$MKLITTLEFS" -c "$STAGING" -s "$SIZE" -p "$PAGE_SIZE" -b "$BLOCK_SIZE" "$OUTPUT" > /dev/null 2>&1
fi
printf "${green}success${reset}\n"

printf "\n${bold}Files in the image${reset}\n"
"$MKLITTLEFS" -l -s "$SIZE" -p "$PAGE_SIZE" -b "$BLOCK_SIZE" "$OUTPUT" | sed 's/^/  /'
printf "\n"

if $FLASH; then
    #
    #   Written with OpenOCD, the way the rest of the project is flashed.  The
    #   configuration is generated because the image and the offset are only known here.
    #
    CONFIG=$(mktemp -t mkfsimage)
    trap 'rm -f "$CONFIG"; rm -rf "$STAGING"' EXIT

    cat > "$CONFIG" <<EOF
source [find interface/ftdi/esp_ftdi.cfg]
source [find board/esp-wroom-32.cfg]

init
reset init
halt

program_esp $OUTPUT $OFFSET verify
reset run

shutdown
EOF

    printf "Writing the image to the board at $OFFSET... "
    if $VERBOSE; then
        openocd -s "${OPENOCD_SCRIPTS:-$scriptdir/share/openocd/scripts}" -f "$CONFIG"
    else
        openocd -s "${OPENOCD_SCRIPTS:-$scriptdir/share/openocd/scripts}" -f "$CONFIG" > /dev/null 2>&1
    fi
    printf "${green}success${reset}\n"
fi

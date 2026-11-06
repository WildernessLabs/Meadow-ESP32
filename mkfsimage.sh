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
PYTHON_FOR_VENV="${PYTHON_FOR_VENV:-python3}"
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
    echo "read from the project's sdkconfig and from the partition table that sdkconfig"
    echo "names, so it cannot drift from what the firmware will try to mount."
    echo ""
    echo "Options:"
    echo "-h | --help             Display this message"
    echo "-p=* | --project=*      Project supplying the geometry (default: test)"
    echo "                        Use --project=. for the application"
    echo "-s=* | --source=*       Directory holding the files (default: <project>/fsimage)"
    echo "-o=* | --output=*       Image to write (default: <project>/littlefs.bin)"
    echo "--block-size=*          LittleFS block size (default: 4096, the flash sector)"
    echo "-f | --flash            Write the image to the board once it has been built"
    echo "--rebuild-tool          Make the littlefs-python environment again even if it is there"
    echo "-v | --verbose          Echo the commands as they run"
    echo ""
    echo "Examples:"
    echo "    ./mkfsimage.sh                       build the test image"
    echo "    ./mkfsimage.sh --flash               build it and write it to the board"
    echo "    ./mkfsimage.sh --project=. --flash   the same for the application"
    echo ""
    echo "Deploying an image to a board from a clean tree:"
    echo "    1. idf.py reconfigure        in the project directory, to fetch the managed"
    echo "                                 littlefs component, which names the tool version"
    echo "    2. ./mkfsimage.sh            build the image and list what went into it"
    echo "    3. ./mkfsimage.sh --flash    write it to the board at the littlefs offset"
    echo "    4. idf.py monitor            reset the board and read the mount back"
    echo ""
    echo "Step 1 is only needed after a full clean, which removes managed_components and"
    echo "with it the tool requirements.  Step 4 is worth the minute it costs: a failed"
    echo "mount is not reported, so the only proof the image arrived is the firmware"
    echo "reading the files back."
    echo ""
    exit 0
fi

PROJECT_DIR="$( cd "$scriptdir/$PROJECT" 2>/dev/null && pwd -P )" || {
    printf "${red}No such project directory: $scriptdir/$PROJECT${reset}\n"
    exit 1
}

[ -z "$SOURCE" ] && SOURCE="$PROJECT_DIR/fsimage"
[ -z "$OUTPUT" ] && OUTPUT="$PROJECT_DIR/littlefs.bin"

SDKCONFIG="$PROJECT_DIR/sdkconfig"
#
#   The image is built with littlefs-python, the tool the managed littlefs component itself
#   uses for littlefs_create_partition_image.  Version 1.5.5 of the component carried the
#   mklittlefs sources; later versions do not, and name the littlefs-python release that
#   matches their littlefs in image-building-requirements.txt.  The tool is installed from
#   that file into an environment of its own under the project's build directory.
#
TOOL_REQUIREMENTS="$PROJECT_DIR/managed_components/joltwallet__littlefs/image-building-requirements.txt"
TOOL_VENV="$PROJECT_DIR/build/mkfsimage_venv"
TOOL_PYTHON="$TOOL_VENV/bin/python"

for required in "$SDKCONFIG" "$TOOL_REQUIREMENTS"; do
    if [ ! -e "$required" ]; then
        printf "${red}Cannot find $required${reset}\n"
        printf "The project has to have been configured at least once before an image can be built.\n"
        printf "Run idf.py reconfigure in $PROJECT_DIR first; see mkfsimage.sh --help.\n"
        exit 1
    fi
done

if [ ! -d "$SOURCE" ]; then
    printf "${red}No such source directory: $SOURCE${reset}\n"
    exit 1
fi

#
#   The partition table is the one the configuration names, not a file assumed to be in
#   the project directory.  A project can build with any table it likes, the tests build
#   with one from the directory above, and a partitions.csv can sit beside the project
#   unused, describing a layout the firmware never had.  Taking the offset from that file
#   would write the image where nothing is going to look for it.
#
PARTITIONS_NAME=$(grep -E "^CONFIG_PARTITION_TABLE_FILENAME=" "$SDKCONFIG" | cut -d= -f2- | tr -d '"')
if [ -z "$PARTITIONS_NAME" ]; then
    printf "${red}No CONFIG_PARTITION_TABLE_FILENAME in $SDKCONFIG${reset}\n"
    exit 1
fi

case "$PARTITIONS_NAME" in
    /*) PARTITIONS="$PARTITIONS_NAME" ;;
    *)  PARTITIONS="$PROJECT_DIR/$PARTITIONS_NAME" ;;
esac

if [ ! -f "$PARTITIONS" ]; then
    printf "${red}Cannot find the partition table $PARTITIONS${reset}\n"
    printf "CONFIG_PARTITION_TABLE_FILENAME names it as $PARTITIONS_NAME.\n"
    exit 1
fi

PARTITIONS="$( cd "$(dirname "$PARTITIONS")" && pwd -P )/$(basename "$PARTITIONS")"

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

#
#   The on-disk version has to be one the firmware writes too.  LittleFS 2.6 and later write
#   version 2.1, which LittleFS 2.5 and earlier (the managed component 1.5.5 and the firmware
#   built with it) cannot mount, and that firmware formats a partition it cannot mount.  The
#   firmware can be held at 2.0 with CONFIG_LITTLEFS_MULTIVERSION, so the image follows
#   whatever it was configured to write.
#
if grep -qE "^CONFIG_LITTLEFS_DISK_VERSION_2_0=y" "$SDKCONFIG"; then
    DISK_VERSION=0x00020000
    DISK_VERSION_NAME=2.0
else
    DISK_VERSION=0x00020001
    DISK_VERSION_NAME=2.1
fi

if [ -z "$OFFSET" ] || [ -z "$SIZE" ] || [ -z "$PAGE_SIZE" ] || [ -z "$NAME_MAX" ]; then
    printf "${red}Could not read the image geometry from the project.${reset}\n"
    printf "  offset=%s size=%s page=%s name_max=%s\n" "$OFFSET" "$SIZE" "$PAGE_SIZE" "$NAME_MAX"
    exit 1
fi

printf "${bold}Image geometry, taken from $PROJECT${reset}\n"
printf "  partition table  : %s\n" "$PARTITIONS"
printf "  partition offset : %s\n" "$OFFSET"
printf "  partition size   : %s\n" "$SIZE"
printf "  page size        : %s\n" "$PAGE_SIZE"
printf "  block size       : %s\n" "$BLOCK_SIZE"
printf "  longest name     : %s\n" "$NAME_MAX"
printf "  on-disk version  : %s\n" "$DISK_VERSION_NAME"
printf "  source directory : %s\n" "$SOURCE"
printf "  image            : %s\n" "$OUTPUT"
printf "\n"

#
#   The environment is made again when asked, when it is missing, or when the component
#   names a different version of the tool from the one it was made with.
#
STAMP="$TOOL_VENV/.requirements"
if [ ! -x "$TOOL_PYTHON" ] || [ ! -f "$STAMP" ] || ! cmp -s "$STAMP" "$TOOL_REQUIREMENTS"; then
    REBUILD_TOOL=true
fi

if $REBUILD_TOOL; then
    printf "Installing %s for this machine... " "$(grep -v '^#' "$TOOL_REQUIREMENTS" | head -1)"
    rm -rf "$TOOL_VENV"
    if $VERBOSE; then
        "$PYTHON_FOR_VENV" -m venv "$TOOL_VENV" && "$TOOL_PYTHON" -m pip install -r "$TOOL_REQUIREMENTS"
    else
        ("$PYTHON_FOR_VENV" -m venv "$TOOL_VENV" && "$TOOL_PYTHON" -m pip install -q -r "$TOOL_REQUIREMENTS") > /dev/null 2>&1
    fi
    cp "$TOOL_REQUIREMENTS" "$STAMP"
    printf "${green}success${reset}\n"
fi

#
#   Build or list an image with the littlefs-python API rather than its command line, which
#   cannot choose the on-disk version.  The superblock records the block size, the block
#   count and the longest name, and the firmware refuses an image whose longest name is
#   longer than its own, so all three come from the project.  The read and program sizes
#   are not recorded on disk; the page size is used for both, as mklittlefs did.
#
#   littlefs_image create|list <image> <size> <block size> <page size> <name max> <disk version> [directory]
#
littlefs_image() {
    "$TOOL_PYTHON" -I - "$@" <<'PYTHON'
import os
import sys

from littlefs import LittleFS

command, image, size, block_size, page_size, name_max, disk_version = sys.argv[1:8]
size = int(size, 0)
block_size = int(block_size, 0)
geometry = dict(block_size=block_size, block_count=size // block_size, read_size=int(page_size, 0),
                prog_size=int(page_size, 0), name_max=int(name_max, 0))

if command == "create":
    source = sys.argv[8]
    fs = LittleFS(disk_version=int(disk_version, 0), **geometry)
    for name in sorted(os.listdir(source)):
        with open(os.path.join(source, name), "rb") as original, fs.open(name, "wb") as copy:
            copy.write(original.read())
    with open(image, "wb") as output:
        output.write(fs.context.buffer)
else:
    fs = LittleFS(mount=False, **geometry)
    with open(image, "rb") as stored:
        fs.context.buffer = bytearray(stored.read())
    fs.mount()
    stat = fs.fs_stat()
    print("on-disk version %d.%d, %d of %d blocks used" % (stat.disk_version >> 16, stat.disk_version & 0xFFFF,
                                                        fs.used_block_count, stat.block_count))
    for name in sorted(fs.listdir("/")):
        print("%8d  %s" % (fs.stat(name).size, name))
PYTHON
}

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
littlefs_image create "$OUTPUT" "$SIZE" "$BLOCK_SIZE" "$PAGE_SIZE" "$NAME_MAX" "$DISK_VERSION" "$STAGING"
printf "${green}success${reset}\n"

printf "\n${bold}Files in the image${reset}\n"
littlefs_image list "$OUTPUT" "$SIZE" "$BLOCK_SIZE" "$PAGE_SIZE" "$NAME_MAX" "$DISK_VERSION" | sed 's/^/  /'
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

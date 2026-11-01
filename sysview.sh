#!/bin/bash
#
#   Capture a SEGGER SystemView trace from the ESP32 over JTAG.
#
#   The firmware must have been built with SystemView (./build.sh --sysview, or
#   test/build.sh --sysview) and flashed.  An ESP-Prog must be connected to the JTAG header.
#
#   OpenOCD is started with the ESP-Prog and ESP32 configurations, the capture is started
#   with "esp sysview_mcore", which writes both cores into one file in SEGGER's multi-core
#   format (SystemView 3.60 or later opens it), and OpenOCD is shut down when the capture
#   ends.  --per-core uses "esp sysview" instead and writes one file per core for an older
#   SystemView.
#
#   Usage: sysview.sh [options]
#
#       --from-boot         Reset the ESP32 and trace from app_main.  The trace cannot
#                           start any earlier: a reset drops the trace connection, so the
#                           ESP32 is stopped at app_main and the capture started there.
#       --elf=FILE          Application ELF that --from-boot finds app_main in (default
#                           build/MeadowComms.elf; use test/build/MeadowComms.elf for the tests).
#       --seconds=N         Stop after N seconds.  Without it the capture runs until Ctrl+C.
#       --output=FILE       Trace file (default trace/meadow-<date>-<time>.SVDat).
#       --per-core          Write one file per core (FILE with -pro and -app before the
#                           extension) for SystemView versions before 3.60.
#       -h | --help         Display this message.
#
#   The OpenOCD configuration can be changed with OPENOCD_CONFIG, which defaults to
#   "-f interface/ftdi/esp_ftdi.cfg -f board/esp-wroom-32.cfg", the same files flash.sh uses.
#
set -e
scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

FROM_BOOT=false
ELF="$scriptdir/build/MeadowComms.elf"
SECONDS_TO_RUN=0
OUTPUT=""
PER_CORE=false
OPENOCD_CONFIG=${OPENOCD_CONFIG:-"-f interface/ftdi/esp_ftdi.cfg -f board/esp-wroom-32.cfg"}
TELNET_PORT=4444

for i in "$@"
do
case $i in
    --from-boot)
    FROM_BOOT=true
    ;;
    --elf=*)
    ELF="${i#*=}"
    ;;
    --seconds=*)
    SECONDS_TO_RUN="${i#*=}"
    ;;
    --output=*)
    OUTPUT="${i#*=}"
    ;;
    --per-core)
    PER_CORE=true
    ;;
    -h|--help)
    sed -n '3,29p' "$0" | sed 's/^#//'
    exit 0
    ;;
    *)
    echo "Unknown option ($i).  Run sysview.sh --help for the valid options."
    exit 1
    ;;
esac
done

if ! command -v openocd > /dev/null; then
    echo "openocd was not found.  Set up ESP-IDF in this shell first."
    exit 1
fi

if $FROM_BOOT; then
    if [ ! -f "$ELF" ]; then
        echo "--from-boot needs the application ELF to find app_main, and $ELF does not exist."
        exit 1
    fi
    APP_MAIN=$(xtensa-esp32-elf-nm "$ELF" | awk '$3 == "app_main" { print $1 }')
    if [ -z "$APP_MAIN" ]; then
        echo "app_main was not found in $ELF."
        exit 1
    fi
fi

if [ -z "$OUTPUT" ]; then
    mkdir -p "$scriptdir/trace"
    OUTPUT="$scriptdir/trace/meadow-$(date +%Y%m%d-%H%M%S).SVDat"
fi
case "$OUTPUT" in
    /*) ;;
    *) OUTPUT="$(pwd)/$OUTPUT" ;;
esac
mkdir -p "$(dirname "$OUTPUT")"

if $PER_CORE; then
    base="${OUTPUT%.*}"
    extension="${OUTPUT##*.}"
    START_COMMAND="esp sysview start file://$base-pro.$extension file://$base-app.$extension 1"
    STOP_COMMAND="esp sysview stop"
else
    START_COMMAND="esp sysview_mcore start file://$OUTPUT 1"
    STOP_COMMAND="esp sysview_mcore stop"
fi

#
#   Send one command to OpenOCD's telnet port and wait for the prompt that follows it.
#
openocd_command() {
    python3 - "$TELNET_PORT" "$1" <<'EOF'
import socket, sys, time
port, command = int(sys.argv[1]), sys.argv[2]
connection = socket.create_connection(("localhost", port), timeout=10)
connection.settimeout(10)
received = b""
while not received.rstrip().endswith(b">"):
    received += connection.recv(4096)
connection.sendall(command.encode() + b"\n")
received = b""
deadline = time.time() + 30
while time.time() < deadline and not received.rstrip().endswith(b">"):
    try:
        received += connection.recv(4096)
    except socket.timeout:
        break
connection.close()
output = received.decode(errors="replace").replace("\r", "").split("\n")
print("\n".join(line for line in output[1:-1] if line.strip()))
EOF
}

OPENOCD_LOG="${OUTPUT%.*}.openocd.log"
openocd $OPENOCD_CONFIG -c "telnet_port $TELNET_PORT" -c "gdb_port disabled" -c "tcl_port disabled" > "$OPENOCD_LOG" 2>&1 &
OPENOCD_PID=$!

stop_capture() {
    trap - INT TERM
    echo ""
    echo "Stopping the capture."
    openocd_command "$STOP_COMMAND" || true
    openocd_command "shutdown" > /dev/null 2>&1 || true
    wait $OPENOCD_PID 2> /dev/null || true
    if $PER_CORE; then
        ls -l "${OUTPUT%.*}"-*.${OUTPUT##*.}
    else
        ls -l "$OUTPUT"
    fi
    echo "OpenOCD output is in $OPENOCD_LOG."
    exit 0
}

#
#   Wait for OpenOCD to open its telnet port.
#
for attempt in $(seq 1 50); do
    if ! kill -0 $OPENOCD_PID 2> /dev/null; then
        echo "OpenOCD stopped before it was ready:"
        tail -20 "$OPENOCD_LOG"
        exit 1
    fi
    if nc -z localhost $TELNET_PORT 2> /dev/null; then
        break
    fi
    sleep 0.2
done

if $FROM_BOOT; then
    #
    #   Reset, run to app_main on a hardware breakpoint, and start the capture there.
    #
    openocd_command "reset halt"
    openocd_command "bp 0x$APP_MAIN 2 hw"
    openocd_command "resume"
    openocd_command "wait_halt 10000"
    openocd_command "rbp 0x$APP_MAIN"
fi
echo "Starting the capture to $OUTPUT."
openocd_command "$START_COMMAND"
if $FROM_BOOT; then
    openocd_command "resume"
fi

trap stop_capture INT TERM
if [ "$SECONDS_TO_RUN" -gt 0 ]; then
    sleep "$SECONDS_TO_RUN"
else
    echo "Press Ctrl+C to stop."
    while kill -0 $OPENOCD_PID 2> /dev/null; do
        sleep 1
    done
fi
stop_capture

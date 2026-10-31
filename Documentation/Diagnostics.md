# ESP32 Diagnostics

This page describes the diagnostic output the firmware can produce and how to turn each
part of it on.

## Serial Output

Log output goes to UART0 (GPIO 1 Tx, GPIO 21 Rx) at 115,200 baud through the ESP-IDF
logging library. `Logging::Setup` turns every component off (`ESP_LOG_NONE`) and then
turns on only the ones that are wanted, so output appears only when something asks for
it. The STM32 can change which components are logged, and send the output over UDP
instead, with `Logging::Configure` (issue #356).

### Trace, debug and error messages

`Logging.hpp` defines `TRACE_MESSAGE`, `DEBUG_MESSAGE` and `ERROR_MESSAGE` (and hex
buffer versions of each). They take a `printf` style format and log under the
`COMPONENT_NAME` of the class they are used in. They compile to nothing unless the build
defines `TRACE_MESSAGES`:

```bash
./build.sh --develop --tracemessages --build
```

`--debug=WIFI,SPI,BT,SYSTEM` adds the per-subsystem defines that some classes use for
more detailed output; see `./build.sh --help`.

The diagnostic task and the heap reports use `ESP_LOGI` directly, so their output does
not depend on `--tracemessages`.

## Build Options

The options are in the **Meadow COMMS Project Build Configuration** menu of
`idf.py menuconfig`. They are defined in `components/F7Services/Kconfig.projbuild`, so
the application and the test project both see them.

| Option | Default | Effect |
|---|---|---|
| `CONFIG_ENABLE_DIAGNOSTIC_THREAD` | n | Starts the diagnostic task described below |
| `CONFIG_DIAGNOSTICS_INTERVAL_MS` | 30000 | How often the diagnostic task reports (10,000 to 60,000 ms) |
| `CONFIG_ENABLE_HEAP_INTEGRITY_CHECKS` | n | Checks the heaps each time a message is queued for the STM32 and in each heap report, breaking to the debugger on corruption |
| `CONFIG_TRACE_RECORD_NUM` | 100 | Number of records standalone heap tracing keeps |

`sdkconfig.develop` (merged by `--develop` and by the test build) sets each of these
once, with a note on how to change it.

## Diagnostic Task

With `CONFIG_ENABLE_DIAGNOSTIC_THREAD` set, `SystemRequestHandler` starts a task that
every `CONFIG_DIAGNOSTICS_INTERVAL_MS` logs:

* the time since boot;
* a table of every task: number, core, name, state, current and base priority, stack
  high-water mark, the CPU time it has used **since the last report** and that time as
  a percentage of one core;
* the load on each core, worked out from the time its idle task ran;
* the heap summary from `Heap::ReportUsage`, and the heap use of each task when
  `CONFIG_HEAP_TASK_TRACKING` is set.

The percentages are of one core, so across all tasks they add up to about 100 times the
number of cores. The CPU times come from the FreeRTOS run-time statistics, which
`sdkconfig.develop` turns on (`CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS`, counted with
`esp_timer` in microseconds). In a build without them the CPU columns read 0.

The task runs at priority 1 so that it does not displace the tasks it is measuring.

## Heap

`Heap::ReportUsage` logs the total, free and allocated heap, the number of free blocks,
the largest free block, the lowest the free heap has been since boot, and the
fragmentation (how much of the free heap is outside the largest block). With
`CONFIG_ENABLE_HEAP_INTEGRITY_CHECKS` it checks the heaps first.

### Heap Tracing

`Heap` wraps the ESP-IDF heap tracing API: `Heap::Setup`, `StartTracing`, `StopTracing`,
`DumpTraceRecords` and `ReportStatistics`. Standalone tracing
(`CONFIG_HEAP_TRACING_STANDALONE`, on in `sdkconfig.develop`) keeps
`CONFIG_TRACE_RECORD_NUM` records on the device. Tracing to the host sends every
allocation and free over JTAG instead; see
[Heap Tracing to the Host](#heap-tracing-to-the-host).

## Tracing with SystemView

[SEGGER SystemView](https://www.segger.com/products/development-tools/systemview/) shows
what each core was running and when: every task switch, every interrupt, every timer
callback and every blocking FreeRTOS call, on one time line per core. It answers
questions the log cannot, such as why a request took 40 ms, which task kept another one
waiting, or how much of a core an interrupt takes.

ESP-IDF provides the target side (the `app_trace` component). The trace leaves the
ESP32 over JTAG through the ESP-Prog, and OpenOCD writes it to a file that SystemView
opens.

### Hardware

An ESP-Prog connected to the 10-pin JTAG header, as for flashing over JTAG (see
[Tools](Tools.md)). The JTAG pins are GPIO 12 to 15; the SPI link to the STM32 uses
GPIO 5, 18, 19 and 23 and the log UART GPIO 1 and 21, so tracing does not disturb either.

The ESP-Prog enumerates as two serial ports. The first is the JTAG channel, which
OpenOCD uses; the second is the console.

### Building

```bash
./build.sh --build --fullclean --develop --sysview      # or without --develop
./build.sh --flash
```

`--sysview` merges `sdkconfig.sysview` after the other configuration files. It turns on
application tracing to JTAG and SystemView with all of the FreeRTOS events, and it moves
the heap, ring buffer and FreeRTOS API functions out of IRAM to make room for the trace
hooks (about 7.3 KB of IRAM): without that, both the release and the develop build
overflow IRAM. A traced build's heap and FreeRTOS calls can therefore be slightly slower
than an untraced build's, because they can miss the flash cache.

Change to or from a SystemView build with `--fullclean`, because the root `sdkconfig` is
kept between builds.

The tests are traced the same way: `test/build.sh --build --fullclean --sysview`.

### Capturing

Set up ESP-IDF in the shell (OpenOCD comes with it), then from the root of the
repository:

```bash
./sysview.sh                     # trace the running application until Ctrl+C
./sysview.sh --seconds=10        # trace for ten seconds
./sysview.sh --from-boot         # reset, run to app_main, trace from there
./sysview.sh --output=run.SVDat  # choose the file (default trace/meadow-<date>-<time>.SVDat)
```

The script starts OpenOCD with the same configuration as `flash.sh`
(`interface/ftdi/esp_ftdi.cfg` and `board/esp-wroom-32.cfg`; change it with
`OPENOCD_CONFIG`), starts the capture, and shuts OpenOCD down at the end. OpenOCD's
output goes next to the trace file as `<name>.openocd.log`; it ends with the size of the
trace and the number of bytes lost.

`--from-boot` cannot start tracing at the reset itself, because the reset drops the
trace connection. Instead it stops the ESP32 at `app_main` on a hardware breakpoint and
starts the capture there, which still shows every application task being created. It
reads the address of `app_main` from `build/MeadowComms.elf`; pass
`--elf=test/build/MeadowComms.elf` to trace the tests.

If the capture reports a size of 0 and OpenOCD's log shows `esp32.cpu1` failing
examination, the ESP32 is being held in reset, usually by a serial monitor or esptool
having left the reset line asserted. Open and close the console port (or press the
reset button) and try again.

### Viewing

The capture uses `esp sysview_mcore`, which writes both cores into one file in SEGGER's
multi-core format. Open it with SystemView 3.60 or later. `./sysview.sh --per-core` writes
one file per core instead, for older versions of SystemView.

ESP-IDF can also summarise a trace without SystemView:

```bash
python $IDF_PATH/tools/esp_app_trace/sysviewtrace_proc.py -b build/MeadowComms.elf file://trace/<file>.SVDat
python $IDF_PATH/tools/esp_app_trace/sysviewtrace_proc.py -d -b build/MeadowComms.elf file://trace/<file>.SVDat   # every event
```

### Meadow Events

A SystemView build also records what the firmware is doing, as the events of a Meadow
SystemView module (`components/F7Services/Diagnostics/Profile.hpp`):

| Event | Values | Recorded |
|---|---|---|
| 0 `SpiTransaction` | SPI state, bytes received | When an SPI transaction with the STM32 finishes |
| 1 `SpiProcess`, 2 `SpiProcessDone` | SPI state | Around the handling of that transaction |
| 3 `RequestReceived` | message ID, interface, function | When a whole request reaches the dispatcher |
| 4 `HandleRequest`, 5 `HandleRequestDone` | message ID, interface, function (the end: message ID) | Around a request handler's work on the request |
| 6 `MessageQueued` | message ID, message type, messages on the queue | When a response or event is queued for the STM32 |
| 7 `MessageAcknowledged` | message ID | When the STM32 acknowledges the last packet of a message |

A request's life is also marked with a SystemView marker keyed on its message ID, from
`RequestReceived` to the `MessageAcknowledged` of its response, which SystemView shows as
one span across tasks.

The events are on with `CONFIG_MEADOW_SYSVIEW_EVENTS`, which a `--sysview` build sets;
without SystemView the calls compile to nothing. The firmware describes the module to
SystemView itself. Copy `tools/diagnostics/SYSVIEW_Meadow.txt` into SystemView's
`Description` folder (on a Mac, `/Applications/SEGGER/SystemView/Description`) to see the
names of the interfaces, message types and SPI states rather than their numbers.

SystemView numbers a module's events from where the module was registered, so the IDs
in a trace depend on what else registered first: with heap tracing the heap module takes
512 and 513 and the Meadow events start at 514. ESP-IDF's `sysviewtrace_proc.py` prints
them as `SysViewEvent(<id>)` with their raw values.

A span ends with an event of its own (`SpiProcessDone`, `HandleRequestDone`) rather than
SystemView's end-of-call record, because ESP-IDF's `sysviewtrace_proc.py` cannot read
that record ("Unknown event ID 28"), and that would stop it producing the heap leak
report.

### The Log in SystemView

In a SystemView build, `Logging::Configure` with the JTAG destination
(`Esp32LogDestination::Jtag`) sends the log to the trace instead of the UART, where
SystemView shows it in its terminal window on the same time line as the tasks and
events. Any other destination puts the UART back. Without SystemView, JTAG is refused
with `NotImplemented` as before.

### Heap Tracing to the Host

```bash
./build.sh --build --fullclean --develop --sysview --heaptrace
./build.sh --flash
./sysview.sh --from-boot --seconds=30 --output=heap.SVDat
python $IDF_PATH/tools/esp_app_trace/sysviewtrace_proc.py -p -i heap -b build/MeadowComms.elf file://heap.SVDat
```

`--heaptrace` (only with `--sysview`; the tests take it too) merges `sdkconfig.heaptrace`,
which replaces standalone heap tracing with tracing to the host: every allocation and
free, with the four functions that called it, becomes a SystemView event while a capture
is running. Tracing starts by itself when a capture starts (`Heap::Setup`, called by
`SystemRequestHandler`, starts it from a task of its own, because `heap_trace_start`
waits for the capture). `sysviewtrace_proc.py -p -i heap` then lists the allocations the
capture saw that were not freed during it, with their callers, and ends with a line such
as "Found 90 leaked bytes in 2 blocks". An allocation still in use when the capture ends
is counted, so read the list rather than the total: a leak is an allocation that keeps
appearing as the capture gets longer.

`gdbinit.tohost` does the same from GDB, starting the capture at `heap_trace_start`.

### Limits

* JTAG carries a little over 200 KiB/s of trace. A busy system makes events faster than
  that: tracing the `[Emulated]` tests on the bench board gave 5.3 MB in 30 seconds with
  214 overflows on each core. A task waits up to 500 us
  (`CONFIG_APPTRACE_SV_BUF_WAIT_TMO`) for room in the trace buffer, after which the event
  is dropped and SystemView shows an overflow. The time line stays correct around an
  overflow, but the events in it are missing. Turning off events you do not need in
  `sdkconfig.sysview` (the idle and timer events, say) leaves room for the rest.
* SystemView over a UART instead of JTAG exists in ESP-IDF but traces only one core, and
  is not set up here.

## Core Dumps

A crash saves a core dump to the `coredump` partition, which the STM32 can read back.
See `CoreDump.hpp` and the core dump tests.

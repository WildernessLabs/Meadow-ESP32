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

The diagnostic snapshots use `ESP_LOGI` directly, so their output does not depend on
`--tracemessages`.

## Build Options

The options are in the **Meadow COMMS Project Build Configuration** menu of
`idf.py menuconfig`. They are defined in `components/F7Services/Kconfig.projbuild`, so
the application and the test project both see them.

| Option | Default | Effect |
|---|---|---|
| `CONFIG_MEADOW_DIAGNOSTICS_PERIOD_MS` | 0 | Time between diagnostic snapshots; 0 for none (see below) |
| `CONFIG_ENABLE_HEAP_INTEGRITY_CHECKS` | n | Checks the heaps each time a message is queued for the STM32, breaking to the debugger on corruption |
| `CONFIG_TRACE_RECORD_NUM` | 100 | Number of records standalone heap tracing keeps |

`sdkconfig.develop` (merged by `--develop` and by the test build) sets each of these
once, with a note on how to change it.

## Diagnostic Snapshots

A snapshot describes the whole system at one moment, as a set of log lines called
records. Each record starts `@diag`, holds comma separated values, and carries the
sequence number of its snapshot, so the records can be picked out of any log and put in
a spreadsheet or graphed (see Host Tools below). They go wherever the log goes: the UART,
UDP, or SystemView.

Snapshots are taken:

* every `CONFIG_MEADOW_DIAGNOSTICS_PERIOD_MS` milliseconds (0, the default, for none; at
  least 1,000) by a task that `SystemRequestHandler` starts. The task runs just above idle,
  so taking a snapshot does not push aside the tasks it is measuring, and it sleeps until
  the period ends or a snapshot is asked for;
* when asked for with `Diagnostics::RequestSnapshot()` (taken by that task);
* at once, in the calling task, with `Diagnostics::TakeSnapshot()`, which the tests use.

To have one every ten seconds in a develop build, set
`CONFIG_MEADOW_DIAGNOSTICS_PERIOD_MS=10000` in `sdkconfig.develop` and rebuild with
`--fullclean`.

### Records

| Record | Values after the sequence number |
|---|---|
| `snapshot` | time since boot (ms), reset reason (`esp_reset_reason`), free heap |
| `task` (one per task) | task number, name, core (-1 for either), state, priority, base priority, stack never used (bytes), CPU time since the last snapshot (us), that time as % of one core |
| `cpu` | length of the interval (us), load on core 0 (%), load on core 1 (%) |
| `heap` (internal, dma; spiram with PSRAM) | kind of memory, total, free, lowest free since boot, largest free block, free blocks, fragmentation (% of the free memory outside the largest block) |
| `taskheap` (with `CONFIG_HEAP_TASK_TRACKING`) | task, alive (1) or deleted (0), bytes in use, most bytes in use |
| `counter` | name, value |
| `gauge` | name, value, highest value |
| `histogram` | name, count, total (us), longest (us), then the count in each of the 24 buckets |
| `network` (link, ip, tcp, udp; with `CONFIG_LWIP_STATS`) | protocol, packets sent, received, dropped, out of memory, other errors |
| `end` | time the snapshot took (us) |

For example, from the bench board with no STM32 attached (some task records left out):

```
I (307403) Diagnostics: @diag,snapshot,30,306764,1,146068
I (307403) Diagnostics: @diag,task,30,12,Diagnostics,-1,Running,1,1,1460,238412,2
I (307403) Diagnostics: @diag,task,30,6,IDLE1,1,Ready,0,0,1192,10230000,100
I (307403) Diagnostics: @diag,task,30,5,IDLE0,0,Ready,0,0,1296,9991588,97
I (307443) Diagnostics: @diag,task,30,9,SPI,-1,Blocked,16,16,7524,0,0
I (307473) Diagnostics: @diag,cpu,30,10229995,3,0
I (307483) Diagnostics: @diag,heap,30,internal,240100,150180,149428,110580,6,27
I (307493) Diagnostics: @diag,heap,30,dma,235968,146068,145360,110580,5,25
I (307523) Diagnostics: @diag,counter,30,EventsQueued,1
I (307543) Diagnostics: @diag,gauge,30,OutboundQueueDepth,1,1
I (307553) Diagnostics: @diag,histogram,30,WiFiRequestLatency,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
I (307623) Diagnostics: @diag,network,30,tcp,0,0,0,0,0
I (307633) Diagnostics: @diag,end,30,234522
```

A snapshot of about 35 records takes about a quarter of a second, nearly all of it waiting
on the UART at 115,200 baud. Taking one every ten seconds kept the snapshot task at 2%
of a core.

The CPU times come from the FreeRTOS run-time statistics, which `sdkconfig.develop` turns
on (`CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS`, counted with `esp_timer` in microseconds).
They cover the time since the previous snapshot, not since boot, so they show what the
system has been doing lately. The percentages are of one core, so across all tasks they
add up to about 100 times the number of cores, and the load on a core is worked out from
the time its idle task ran. In a build without the statistics, a release build for
example, the CPU values are 0. `sdkconfig.develop` also turns on the lwIP statistics
(`CONFIG_LWIP_STATS`) for the `network` records.

Internal memory (all the on-chip RAM the heap uses) and DMA capable memory (the data RAM)
are reported because they are what run out first on this part: WiFi, Bluetooth and the
SPI DMA buffers all need them. Without PSRAM, 8-bit and 32-bit capable memory are the same
as these two, so they are not reported separately.

### Host Tools

`tools/diagnostics/parse_diag.py` reads any file holding the records (a monitor log, a
`run-tests.sh` transcript, a capture of the UDP log), writes one CSV file per kind of
record with the time since boot added to each row, and prints a summary of the last
snapshot: CPU load, the busiest tasks, the tasks with least stack left, the heap, the
counters, and for each histogram the count, mean, 50th, 90th and 99th percentiles (to the
bucket) and longest time. It needs only the Python standard library.

```bash
python3 tools/diagnostics/parse_diag.py monitor.log                # CSV files in diag-monitor/
python3 tools/diagnostics/parse_diag.py monitor.log --output soak  # CSV files in soak/
```

`tools/diagnostics/plot_diag.py` draws the free and lowest free heap, the load on each
core and the stack left in each task over time, and the latest histograms, as PNG files.
It needs matplotlib (`pip install matplotlib`).

```bash
python3 tools/diagnostics/plot_diag.py monitor.log
```

A snapshot's sequence number starts again at 1 after a reset, and the tools number the
boots, so one log can span several.

## Heap

`Heap` checks the heaps (`Heap::CheckIntegrity`, with
`CONFIG_ENABLE_HEAP_INTEGRITY_CHECKS`) and wraps heap tracing. The heap's state is in the
`heap` records of a snapshot.

### Heap Tracing

`Heap` wraps the ESP-IDF heap tracing API: `Heap::Setup`, `StartTracing`, `StopTracing`,
`DumpTraceRecords` and `ReportStatistics`. Standalone tracing
(`CONFIG_HEAP_TRACING_STANDALONE`, on in `sdkconfig.develop`) keeps
`CONFIG_TRACE_RECORD_NUM` records on the device. Tracing to the host sends every
allocation and free over JTAG instead; see
[Heap Tracing to the Host](#heap-tracing-to-the-host).

## Metrics

`Metrics` (`components/F7Services/Diagnostics/Metrics.hpp`) keeps a small, fixed set of
numbers about the firmware in every build, release included, so that there is something
to look at without JTAG or a special build. Recording one is a few atomic operations on
statically allocated memory: no allocation, no lock and no formatting. Turn them off with
`CONFIG_MEADOW_METRICS` (on by default) to compile the calls away.

| Kind | Name | What |
|---|---|---|
| Counter | `SpiTransactions` | SPI transactions with the STM32 that finished |
| Counter | `RequestsReceived` | Whole requests that reached the dispatcher |
| Counter | `ResponsesQueued`, `EventsQueued` | Messages put on the queue for the STM32 |
| Counter | `MessagesDropped` | Messages dropped because the queue stayed full |
| Counter | `AllocationFailures` | Heap allocations that failed |
| Gauge | `OutboundQueueDepth` | Messages waiting for the STM32 now, and the most there have been |
| Gauge | `FailedAllocationSize` | Size of the last failed allocation, and the largest |
| Histogram | `WiFiRequestLatency`, `BluetoothRequestLatency`, `SystemRequestLatency` | Time from a request arriving to the STM32 acknowledging the last packet of its response |
| Histogram | `WiFiHandlerTime`, `BluetoothHandlerTime`, `SystemHandlerTime` | Time a request handler spends on a request |

A histogram has 24 buckets: bucket 0 holds 0 us and bucket n holds 2^(n-1) us up to 2^n us,
the last holding everything from about 4.2 seconds up. Each also keeps the count, the
total and the longest time.

A request is timed only if its handler sends the request back as the response, which
most do; a response built as a new message carries no arrival time.

Failed allocations are counted from `heap_caps_register_failed_alloc_callback`. The heap
takes one callback, so the develop build's report of each failure (`AppMain.cpp`) is
passed to `Metrics::Setup` and called from the metrics' callback.

The per-function detail the old performance logging recorded (`--performancelogging`, now
removed) comes from SystemView instead: the `HandleRequest` span times every request with
its interface and function.

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

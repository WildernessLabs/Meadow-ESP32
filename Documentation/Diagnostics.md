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
`CONFIG_TRACE_RECORD_NUM` records on the device. Tracing to the host
(`CONFIG_HEAP_TRACING_TOHOST`) sends them over JTAG; `gdbinit.tohost` starts and stops
the capture at `heap_trace_start` and `heap_trace_stop`.

## Performance Logging

`./build.sh --performancelogging` defines `PERFORMANCE_LOGGING`, which turns on the
`PERFORMANCE_LOGGING_*` macros in `Logging.hpp`. They record how long the instrumented
POSIX handlers, and the round trip of each message to the STM32, take, in a ring of 500
entries that is printed as CSV at the end of `Listen` and `Shutdown`.

## Core Dumps

A crash saves a core dump to the `coredump` partition, which the STM32 can read back.
See `CoreDump.hpp` and the core dump tests.

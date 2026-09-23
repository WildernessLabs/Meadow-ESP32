# Unit Testing Guide

The MeadowComms project includes comprehensive unit tests for the core subsystems. This document describes how to build, run, and understand the test suite.

## Test Overview

The unit tests are located in the `components/F7Services/test/` directory and cover the following subsystems:

| Test File | Coverage |
|-----------|----------|
| `TestWiFi.cpp` | WiFi connectivity and access point scanning |
| `TestBluetooth.cpp` | Bluetooth services, characteristics, and attributes |
| `TestFileSystem.cpp` | File system operations and SPIFFS management |
| `TestSystem.cpp` | System-level functionality and configuration |
| `TestNvsManager.cpp` | Non-volatile storage (NVS) operations |
| `TestCoreDump.cpp` | Core dump functionality |
| `TestApplicationVersion.cpp` | Application version tracking |
| `TestPosix.cpp` | POSIX socket API over the messaging interface |
| `TestHelpers.cpp` | Helper functions for test infrastructure |

## Test Infrastructure

### Mock Classes

The test system uses mock classes to isolate functionality without requiring full system initialization:

- **`MockMessageDispatcher`**: Simulates message dispatch without SPI communication
- **`MockTransportProvider`**: Simulates transport layer without actual hardware

### Shared Test Code

The tests share the following modules rather than repeating the same setup in each file:

- **`TestHarness`**: Message dispatcher lifetime and the methods that send a request and
  wait for its response
- **`TestAssertions`**: Assertions for the message header fields nearly every test checks
- **`TestEncoders`**: The encoding and extraction methods that mirror the `Encoders` class
- **`TestWiFiHelpers`**, **`TestFileSystemHelpers`**, **`TestBluetoothHelpers`**:
  Subsystem setup and the request sequences more than one test needs

## Building the Tests

### Prerequisites

#### Build the application first

The test build depends on the application build having been run at least once. The test
application includes `build_info.h`, which only the root `build.sh` generates, and
`test/main/CMakeLists.txt` adds the root `build/config` directory to its include path.
Neither exists in a freshly cloned repository, so building the tests first fails with
`build_info.h: No such file or directory`.

```bash
# From the root of the repository
./build.sh --build
```

This only needs repeating when the root build directory is removed.

#### Supply the WiFi credentials

The settings the tests use are read from a file on the device rather than compiled in,
so changing a network or a server address needs no rebuild:

```bash
cp test/main/unittest.config.json.template test/fsimage/unittest.config.json
# Edit the file with your WiFi SSID, password and test server
./mkfsimage.sh --flash
```

`mkfsimage.sh` builds `mklittlefs` from the managed components before it builds the
image, so the test project has to have been configured at least once. A `--fullclean`
removes `test/managed_components` and the script stops until `idf.py reconfigure` has
been run in `test/`.

A failed mount is not reported. `FileSystem` is set up with `format_if_mount_failed`, so
a partition it cannot read is quietly replaced with an empty one and the only sign is the
settings having gone. The proof that the image arrived is the board reading it back at
startup:

```
#### Unit test configuration #####

I (1164) FileSystem: Setting up file system
  Read from unittest.config.json

  WiFiNetwork                : YourNetwork
  ...
```

A run that says no configuration file was found, straight after one was written, means
the mount failed rather than the file being absent.

`unittest.config.json` carries:

| Entry | |
|---|---|
| `WiFiNetwork`, `WiFiPassword` | the access point the tests join |
| `WebServerIpAddress`, `WebServerPort` | the server the POSIX tests fetch from |
| `SimpleWebServerName`, `SimpleWebPage`, `BinaryResourceName`, `LargeTestFile` | resources on that server |
| `PosixIterations`, `ScanIterations`, `BluetoothRestartIterations` | how many times each of the repeating tests repeats |
| `SoakHeapAllowance`, `SoakProgressInterval` | how far the free heap may fall across a soak test, and how often the soak tests report |

> **Important**: `unittest.config.json` is in `.gitignore` so credentials are not
> committed. The template it is copied from is in source control.

Nothing has to exist for the build to succeed. A run with no configuration file says so
and uses the defaults, which is enough for every test that needs no settings. An entry
the file leaves out keeps its default. A file that is there but cannot be parsed stops
the run before any test starts, rather than testing against settings nobody chose.

The three counts are separate because the tests they belong to are not alike: a page
fetch takes a moment, a scan takes seconds and a Bluetooth restart takes longer still,
so one number for all three would make one test too long or another too short to mean
anything. `SoakHeapAllowance` goes with `PosixIterations` — a leak shows up in
proportion to the number of fetches, so shortening the soak without lowering the
allowance leaves a check that passes whatever happens. The Bluetooth restart test and the
Bluetooth write and notify soaks apply the same allowance to their own counts,
`BluetoothRestartIterations` and the client's `--soak-iterations`.

### Build Commands

Build the test application using the `test/build.sh` script:

```bash
# Basic test build
cd test
./build.sh --build

# Full clean and rebuild
./build.sh --fullclean

# Build and flash (using the serial interface)
./build.sh --build --flash

# Build and generate a QEMU image so the tests can run without hardware
./build.sh --build --qemu
```

The build script in the test directory supports the same options as the main `build.sh` script. See [Building](Building.md) for complete option documentation.

### The Development Configuration

The application only picks up `sdkconfig.develop` when `build.sh` is given `--develop`.
The tests always build with it, so there is no `--develop` option in the test build
script and no test build without it. What it turns on is what the tests need:

- the partition table in `partitions-develop.csv`, which gives the core dump partition
  512 KB in place of 64 KB, and `CONFIG_ESP_COREDUMP_CAPTURE_DRAM`, which puts the RAM
  contents in the dump. Together these make the dump the `[CoreDump]` tests read back
  around 166 KB rather than the 23 KB a dump of stack frames alone comes to, which is
  the size the retrieval has to work at;
- heap poisoning and the assertions, so that a test that corrupts the heap or breaks an
  invariant is caught where it happens rather than somewhere later.

`sdkconfig.develop` names the partition table file relative to the project directory,
and the tests build from a project directory of their own, so `test/sdkconfig.develop`
points it back at the one file in the root of the repository. That file holds nothing
else: the development configuration itself is not copied, so the tests and the
application cannot drift apart.

### Test Build Output

The test build creates a test application with an interactive menu system for selecting and running individual tests:

```
build/
├── MeadowComms.bin       # Test application binary
├── MeadowComms.elf       # Test application with debug symbols
└── ...                   # Other build artifacts
```

## Running Tests Automatically

`test/run-tests.sh` drives the test menu so the tests can be run without anyone typing
at a terminal. The same script covers both a connected board and the emulator, because
the firmware presents the same menu either way.

```bash
cd test

# Every test, under QEMU
./run-tests.sh

# A single tag
./run-tests.sh '[FileSystem]'

# A single test, by name (keep the quotes)
./run-tests.sh '"Delete file"'

# Several selections in order
./run-tests.sh '[NvsManager]' '[FileSystem]'

# On a connected board instead of the emulator
./run-tests.sh --port=/dev/cu.usbserial-101 '[WiFi]'
```

The board enumerates as a pair of ports and only the second of the pair is the console,
so `ls /dev/cu.usbserial-*` and take the higher of the two. The names are assigned by
USB location and change when the board is moved to another socket, so check rather than
remembering them.

Selectors are passed to the menu unchanged, so anything the menu accepts works: a tag
such as `[FileSystem]`, a quoted test name, a menu number, or `*` for everything.

### Options

| Option | Effect |
|---|---|
| `--hardware` | Run on a board rather than under QEMU |
| `-p=* \| --port=*` | Serial device of the board; implies `--hardware` |
| `--no-fresh` | Skip the reset to a known state before running |
| `--timeout=*` | Seconds allowed for each selector (default 600) |
| `--transcript=*` | Write the full console transcript to a file |
| `--junit=*` | Write the results as a JUnit report to a file |
| `--expect-reboot=*` | Selector that restarts the device on purpose; repeatable |
| `--bluetooth-python=*` | Python with `bleak` installed, used to run the Bluetooth clients |
| `--soak-iterations=*` | Iterations the Bluetooth soak clients ask for (default 1000) |
| `-v \| --verbose` | Echo the console output as the tests run |

`--expect-reboot` is what separates a restart that is the point of the test from one that
is a panic. A selector named this way is allowed to take the device down and the run
carries on with the next selector; any other reboot ends the run with status 3. Give the
option once per selector that does it.

### Bluetooth clients

Some `[Bluetooth]` tests wait for a client to connect over the air, and print "Please
connect client within 60 seconds." when their service is advertising. When a test with a
registered client does that, the runner starts the client from `test/SupportScripts`
alongside it on the host's own Bluetooth adapter. The table of which client goes with which
test is `BLUETOOTH_CLIENTS` in `TestRunner.py`.

| Test | Client |
|---|---|
| Notify characteristic (MeadowC, no company identifier) | `NotifyCharacteristicClient.py --name MeadowC` |
| Notify characteristic, multiple descriptors (MeadowD, no company identifier) | `NotifyCharacteristicClient.py --name MeadowD` |
| Soak test writing numeric characteristic, number of iterations controlled by client (MeadowF7, Espressif) | `WriteNumberCharacteristicSoakTest.py` |
| Soak test notifying characteristic, number of iterations controlled by client (MeadowF7NotifyTest, Espressif) | `NotifyCharacteristicSoakTest.py` |

The clients use [Bleak](https://github.com/hbldh/bleak), which the ESP-IDF Python
environment does not include. Keep it in a virtual environment of its own and point the
runner at it:

```bash
python3 -m venv ~/.venvs/meadow-ble
~/.venvs/meadow-ble/bin/pip install -r test/SupportScripts/requirements.txt

cd test
./run-tests.sh --port=/dev/cu.usbserial-101 --bluetooth-python=$HOME/.venvs/meadow-ble/bin/python \
    '"Soak test writing numeric characteristic, number of iterations controlled by client (MeadowF7, Espressif)"'
```

A client that fails fails its selector, and its output is printed. On macOS the terminal
application needs Bluetooth permission (System Settings, Privacy & Security, Bluetooth)
before the client can scan.

A notifying characteristic needs a Client Characteristic Configuration descriptor
(`0x2902`, two bytes) in its configuration. The firmware does not add one, and without it
a client cannot subscribe; CoreBluetooth reports "attribute not found".

By default each run starts from a known state: under QEMU the pristine flash image is
restored, and on hardware the board is reset. Use `--no-fresh` to continue from
whatever the previous run left behind, which is occasionally useful when reproducing
an ordering problem.

### Exit Status

The exit status distinguishes the ways a run can end, so it can be used in a script or
a build pipeline:

| Status | Meaning |
|---|---|
| 0 | Every selector passed |
| 1 | A test reported a failure |
| 2 | A selector produced no result before the timeout |
| 3 | The device rebooted part way through, usually a panic |

## Running Under QEMU

QEMU runs the firmware without any hardware, which makes it suitable for the file
system and storage tests and for use in a build pipeline. It emulates the processor,
flash, UART and timers, but not the radio and not the ADC.

| Tests | Runs under QEMU |
|---|---|
| `[NvsManager]` | Yes |
| `[FileSystem]` | Yes, though one test is tagged `[Slow]` and left out of CI |
| `[CoreDump]` | Yes — the emulator saves a dump and reads it back as the hardware does, including a full retrieval |
| `[System]` | No — reads a real ADC, and the read never returns under QEMU |
| `[WiFi]`, `[Bluetooth]` | No — there is no radio to emulate |
| `[POSIX]` | No — needs a radio and a web server on the local network |

Everything in that last group carries `[HardwareOnly]`.

### Installing the Emulator

QEMU is an optional ESP-IDF tool and is not installed by default:

```bash
python $IDF_PATH/tools/idf_tools.py install qemu-xtensa
```

Export the ESP-IDF environment afterwards so `qemu-system-xtensa` is on the path.

### Building an Image

QEMU boots from a single image covering the whole of flash rather than from the
separate binaries used when flashing a board. `build.sh --qemu` produces one by merging
the build output, using the flash layout recorded by the build:

```bash
cd test
./build.sh --build --qemu
```

This writes three files into the build directory:

| File | Purpose |
|---|---|
| `qemu_flash.bin` | The image QEMU boots |
| `qemu_flash_pristine.bin` | Reference copy used to reset between runs |
| `qemu_efuse.bin` | eFuse values, which QEMU has no hardware for |

> **Important**: QEMU writes to `qemu_flash.bin` in place, so the file system contents,
> NVS and any core dump survive between runs. `run-tests.sh` restores the pristine copy
> before each run unless `--no-fresh` is given.

The same option works for the application build from the root of the repository.

### Running an Image Interactively

`qemu.sh` boots an image with the console attached to the terminal, which is the
emulator equivalent of `idf.py monitor`:

```bash
# The application image
./qemu.sh

# The test image, from a known starting state
./qemu.sh --test --fresh
```

Press `Ctrl-A` then `X` to leave QEMU.

## Continuous Integration

The `Unit tests` workflow (`.github/workflows/unit-tests.yml`) runs the emulated tests
on every push and pull request against `develop`. It needs no hardware, so it runs on
a standard GitHub runner alongside the existing build and code check workflows.

The workflow itself is a thin wrapper. All of the work is in `run-tests-ci.sh` at the
root of the repository, which runs inside the ESP-IDF container:

```bash
docker run --rm -v $PWD:/project -w /project espressif/idf:release-v5.5 ./run-tests-ci.sh
```

Running that same command locally reproduces a CI failure exactly, which is the reason
the logic lives in a script rather than in the workflow file.

`run-tests-ci.sh` installs the QEMU system libraries and the emulator, builds the
application and then the tests, and finally runs everything carrying the `[Emulated]`
tag, followed by the handful of selectors named in the script that run under QEMU but
cannot join that batch. It writes no configuration file: the emulated tests need none,
and the run reports that none was found.

### How Tests Opt In

Tests carry a second tag describing *how* they run alongside the one saying what they
cover, so the CI job does not need a list of tags to keep in step:

| Tag | Meaning |
|---|---|
| `[Emulated]` | Runs unattended under QEMU; this is what CI selects |
| `[HardwareOnly]` | Cannot run under emulation at all, needing a radio, an ADC or a server |
| `[Slow]` | Too slow to run on every commit |

The three are independent. `[Slow]` says how long a test takes, not where it can
run: `WriteFile returns Failure on a full filesystem` is `[Slow]` but not
`[HardwareOnly]`, because it passes under QEMU in about six minutes against seconds
on a board.

> **Selecting a subsystem includes its slow tests.** Tag matching is a substring
> search, so `[POSIX]` also picks up anything declared `[POSIX][HardwareOnly][Slow]`.
> Selecting `[POSIX]` therefore runs the soak test as well and takes about ten
> minutes, and `[FileSystem]` behaves the same way. Name a single test when only the
> quick ones are wanted:
>
> ```bash
> ./run-tests.sh --port=<port> '"Fetch a web page using recvfrom"'
> ```
>
> The default per selector timeout of 600 seconds is not enough for `[POSIX]` as a
> whole; pass `--timeout=1200`.

`[CoreDump]`, `[Configuration]`, `[MessageDispatcher]`, `[WiFiConfiguration]`,
`[SpiTransportProvider]` and `[Helpers]` carry neither `[Emulated]` nor `[HardwareOnly]`. They run under emulation
but cannot share a batch, so `run-tests-ci.sh` names each of them as a selector of its
own and the comments there say why. The reason is nearly always a singleton: building
a dispatcher or answering a System message claims the request handlers for the rest of
the boot, and a second instance of one resets the board by design. The core dump
sequence is ordered and restarts the device part way through, and the helpers are not
tests in the usual sense.

The order matters. `[Configuration]` runs straight after `[Emulated]`, because the
SystemRequestHandler it builds mounts the file system and would otherwise be holding
the partition while the `[FileSystem]` tests work on it. The core dump tests follow,
and the restart in the first of them hands the singletons back to the selectors after.
`[SpiTransportProvider]` comes last of all: the provider it builds prints `+++RDY` to the
console for the rest of the boot, which can make the runner lose any selector after it
(issue #499).

Tag matching is a substring search, so a test declared as
`"[FileSystem][Emulated]"` is still selected by `[FileSystem]`. Nothing is hidden from
a developer or a hardware run by being tagged.

A new test opts into CI by being declared with `[Emulated]`, and says so when it
needs hardware:

```c
#define CLASS_OF_TESTS "[FileSystem][Emulated]"
#define CLASS_OF_TESTS "[WiFi][HardwareOnly]"
```

Tags can be combined in the negative as well, so `![Slow]` runs everything except
the long tests and `[HardwareOnly]` selects everything needing a board.

`WriteFile returns Failure on a full filesystem` is the one test currently marked
`[Slow]`. It fills the partition a block at a time, which takes minutes on emulated
flash against seconds on hardware, so it runs on the bench and on demand rather than on
every commit:

```bash
cd test
./run-tests.sh --timeout=900 '[Slow]'
```

### Test Results

The run writes a JUnit report and the full console transcript to `test-results/`, and
the workflow publishes both as a build artefact whether the run passed or failed. The
JUnit report lists each test individually with its file and line, so a failure points
straight at the source rather than at the tag that contained it.

To produce the same locally:

```bash
cd test
./run-tests.sh --junit=results.xml --transcript=console.log '[FileSystem]'
```

### Why Docker

Espressif publishes `espressif/esp-idf-ci-action`, but it is a composite action that
runs the same `espressif/idf` container underneath, adding little beyond a tidier
syntax and a `safe.directory` call that `build.sh` already handles for itself. There is
no official action that installs the toolchain natively on a runner, so the container
remains the sensible choice and matches the existing workflows.

## Running Tests Manually

### Starting the Test Application

After flashing the test application, connect to the ESP32 serial port:

```bash
idf.py monitor
```

Or specify the port explicitly:

```bash
idf.py -p /dev/ttyUSB0 monitor
```

On successful startup, you should see:

```
#### Starting interactive test menu #####

Press ENTER to see the list of tests.
```

### Test Menu

Press `ENTER` to display the list of available tests:

```
Available tests:
1. WiFi: Scan for Access Points
2. WiFi: Connect to Access Point
3. Bluetooth: Service Creation
4. File System: Operations
5. System: Configuration
6. NVS: Storage Operations
7. Helper: Restart Board
8. Helper: Enable Heap Tracing to Host
```

### Running Individual Tests

Enter the test number to run that specific test:

```
Select test: 1
```

The test will execute and display results on the serial console. For example:

```
Running: WiFi Scan Test
Scanning for access points...
Found 5 networks:
  1. MyNetwork (Signal: -45dBm)
  2. GuestNetwork (Signal: -60dBm)
  ...
Test completed.
```

### Test Output Format

Test results follow this format:

- **PASS**: Test completed successfully
- **FAIL**: Test failed with assertion error
- **SKIP**: Test was skipped (prerequisites not met)

## Advanced Testing Features

### Helper Tests

The test menu includes helper functions that are not actual tests but provide diagnostic capabilities:

#### Restart Board

Restarts the ESP32. Useful for testing power-on initialization behavior.

#### Enable Heap Tracing to Host

Enables host-based heap tracing for memory profiling. See [Heap Tracing](#heap-tracing) below.

### Heap Tracing

ESP-IDF provides two heap tracing methods:

1. **Host-Based Tracing**: Traces memory allocations to the host computer (recommended)
2. **Device-Based Tracing**: Traces memory allocations stored on the device

#### Using Host-Based Heap Tracing

Host-based heap tracing is useful for identifying memory leaks and tracking heap usage during test execution.

##### Setup

1. Uncomment the `CONFIG_HEAP_TRACING_*` options in `test/sdkconfig.defaults`, then build and flash the test application:
   ```bash
   ./build.sh --build --flash
   ```

2. Ensure you have a debugger connected (OpenOCD):
   ```bash
   # Terminal 1: Monitor output
   idf.py monitor
   
   # Terminal 2: OpenOCD debugging session
   idf.py openocd
   
   # Terminal 3: GDB with initialization
   xtensa-esp32-elf-gdb -x test/gdbinit build/MeadowComms.elf
   ```

3. From the test menu, select the "Enable Heap Tracing to Host" helper
4. Run your test from the menu
5. GDB will pause when the test completes

##### Analyzing Heap Traces

After GDB pauses, you can analyze the heap trace:

```bash
$IDF_PATH/tools/esp_app_trace/sysviewtrace_proc.py -p -b build/MeadowComms.elf heap_log0.svdat > memory.log
```

This generates a `memory.log` file with detailed heap allocation/deallocation information.

##### Cleanup

Exit all three terminal sessions:

```bash
# In Terminal 3 (GDB):
quit

# In Terminal 1 (idf.py monitor):
Ctrl-]

# In Terminal 2 (idf.py openocd):
Ctrl-C
```

### Debugging Under QEMU

QEMU provides its own GDB stub, so a test can be debugged without a JTAG probe. This is
often the quickest way to investigate a panic, because the debugger catches the fault
where it happens rather than leaving a core dump to unpick afterwards.

Start the emulator halted, in one terminal:

```bash
./qemu.sh --test --debug --fresh
```

Attach the debugger in another:

```bash
xtensa-esp32-elf-gdb -x qemu-gdbinit test/build/MeadowComms.elf
```

`qemu-gdbinit` connects to the stub and breaks at `app_main`. Use `continue` to reach
the test menu, then drive the menu from the QEMU terminal as usual.

The port can be moved with `--gdb-port=` if 3333 is already in use.

> **Note**: `test/gdbinit` is for hardware debugging through OpenOCD and issues monitor
> commands that QEMU does not provide. Use `qemu-gdbinit` for the emulator and
> `test/gdbinit` for a board.

### Core Dump Testing

Tests related to core dump functionality verify that crash information is properly captured and can be analyzed. Core dump to flash is already enabled by `sdkconfig.defaults` (`CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH`), so a normal build is all that's needed:

```bash
./build.sh --build
```

After a crash, retrieve and analyze the core dump:

```bash
# Read core dump from flash
idf.py read_flash 0x350000 0x100000 core_dump.bin

# Generate core dump ELF file
espcoredump.py info_corefile -b build/MeadowComms.elf core_dump.bin
```

## Test Development

### Adding New Tests

To add a new test:

1. Create a new test file in `components/F7Services/test/` named `test_<subsystem>.cpp`

2. Include the test infrastructure headers:
   ```cpp
   #include "test_common.hpp"
   #include "cstring"
   ```

3. Create test functions:
   ```cpp
   void test_my_feature()
   {
       // Arrange
       MyClass instance;
       
       // Act
       int result = instance.SomeMethod();
       
       // Assert
       TEST_ASSERT_EQUAL(expected_value, result);
   }
   ```

4. Register the test in the test menu (see `test_common.cpp`)

5. Build and run as described above

### Test Framework

Tests use the **Unity** test framework provided by ESP-IDF. Common assertions include:

```cpp
TEST_ASSERT_EQUAL(expected, actual);           // Equality
TEST_ASSERT_TRUE(condition);                   // Boolean
TEST_ASSERT_NULL(pointer);                     // Null pointer
TEST_ASSERT_EQUAL_STRING(expected, actual);    // String comparison
```

For more assertions, see [Unity Documentation](http://www.throwtheswitch.org/unity).

### Using Mock Classes

Example with `MockMessageDispatcher`:

```cpp
void test_with_mock_dispatcher()
{
    MockMessageDispatcher dispatcher;
    MyHandler handler(dispatcher);
    
    Message request = {/* ... */};
    handler.ProcessRequest(request);
    
    // Verify behavior through mock
    TEST_ASSERT_EQUAL(expected_call_count, dispatcher.GetCallCount());
}
```

### Driving a Request Handler

Requests reach a handler through the dispatcher, the same way they arrive from the STM32.
The handler is created by the dispatcher the first time it has a message to deliver, so
there is nothing for a test to construct itself:

```cpp
TEST_CASE("Scan for access points", "[WiFi][HardwareOnly]")
{
    MockMessageDispatcher *dispatcher = SetupWiFiDispatcher();

    QueueWiFiRequest(dispatcher, WiFiFunction::GetAccessPoints);

    Message *response = WaitForResponseTo(dispatcher, Esp32Interfaces::WiFi, WiFiFunction::GetAccessPoints, RadioTimeout());
    ASSERT_RESPONSE(response, Esp32Interfaces::WiFi, WiFiFunction::GetAccessPoints, StatusCodes::CompletedOk);
    ASSERT_HAS_PAYLOAD(response);

    Message::DeleteMessage(response);
}
```

## Troubleshooting Tests

### Issue: Serial Connection Cannot Be Established

**Solution**: Verify the serial port:
```bash
# List available ports
ls /dev/tty.*  # macOS/Linux
mode COM:       # Windows

# Specify port explicitly
idf.py -p /dev/ttyUSB0 monitor
```

### Issue: `mkfsimage.sh` cannot find the managed components

**Solution**: Configure the test project so the `mklittlefs` sources are fetched again:
```bash
cd test
idf.py reconfigure
```
`--fullclean` removes `test/managed_components`, and `mkfsimage.sh` builds its own copy
of `mklittlefs` from what is there. Reconfiguring brings it back without a full rebuild.

### Issue: The run says no configuration file was found

**Solution**: Build one and write it to the board:
```bash
cp test/main/unittest.config.json.template test/fsimage/unittest.config.json
# Edit with your WiFi credentials and test server
./mkfsimage.sh --flash
```
The tests that need no settings run without one, so this only matters for the WiFi,
Bluetooth and POSIX tests.

If the settings are still missing after an image has been written, the mount is failing
rather than the file being absent, and the empty partition is one the firmware has just
reformatted. Run `./mkfsimage.sh` on its own and check the `partition table` and
`partition offset` it reports against the table the firmware was built with.

### Issue: Test Hangs or Never Completes

**Solution**: 
1. Check the WiFi credentials in `unittest.config.json`
2. Increase test timeout in test file
3. Run with verbose logging:
   ```bash
   ./build.sh --develop --debug=WIFI --build
   ```

### Issue: `[FileSystem]` Takes Several Minutes Under QEMU

This is expected rather than a fault. `WriteFile returns Failure on a full filesystem`
fills the partition a block at a time, and emulated flash is far slower than the real
thing: the test alone takes around six minutes on a developer machine and longer on a
shared build runner, against a few seconds on hardware.

That test is tagged `[Slow]` and is therefore left out of the `[Emulated]` set that CI
runs. Selecting `[FileSystem]` still includes it, so allow for the time or raise the
per-selector timeout:

```bash
./run-tests.sh --timeout=900 '[FileSystem]'
```

### Issue: Build Fails With `build_info.h: No such file or directory`

**Solution**: Build the application before the tests. See
[Prerequisites](#prerequisites).

### Issue: QEMU Reports No Image, or `qemu-system-xtensa` Is Not Found

**Solution**:
1. Generate an image with `./build.sh --build --qemu`
2. Install the emulator with `python $IDF_PATH/tools/idf_tools.py install qemu-xtensa`
3. Export the ESP-IDF environment so the tools are on the path

### Issue: Tests Behave Differently On Consecutive QEMU Runs

**Solution**: QEMU writes to the flash image in place, so state carries over between
runs. Either allow `run-tests.sh` to restore the pristine image, which it does by
default, or restore it by hand with `./qemu.sh --fresh`.

### Issue: Heap Tracing Setup Fails

**Solution**:
1. Ensure debugger is properly connected
2. Verify OpenOCD is running in a separate terminal
3. Check that ESP-IDF tools are properly installed
4. Review [Host-Based Heap Tracing Setup](#using-host-based-heap-tracing)

## Test Documentation

Individual test files contain detailed documentation:

- [WiFi Tests](../../components/F7Services/test/Readme.md#wifi-tests)
- [Bluetooth Tests](../../components/F7Services/test/Readme.md#bluetooth-tests)
- [File System Tests](../../components/F7Services/test/Readme.md#file-system-tests)

## Performance Testing

For performance benchmarks, enable performance logging:

```bash
./build.sh --develop --build -DPERFORMANCE_LOGGING=1
```

Performance metrics are output to the serial console during test execution.

## Next Steps

- [Building the Application](Building.md)
- [Diagnostics and Debugging](Diagnostics.md)
- [Development Tools](Tools.md)

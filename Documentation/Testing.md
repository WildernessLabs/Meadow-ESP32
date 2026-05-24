# Unit Testing Guide

The MeadowComms project includes comprehensive unit tests for the core subsystems. This document describes how to build, run, and understand the test suite.

## Test Overview

The unit tests are located in the `components/F7Services/test/` directory and cover the following subsystems:

| Test File | Coverage |
|-----------|----------|
| `test_wifi.cpp` | WiFi connectivity and access point scanning |
| `test_bluetooth.cpp` | Bluetooth services, characteristics, and attributes |
| `test_file_system.cpp` | File system operations and SPIFFS management |
| `test_system.cpp` | System-level functionality and configuration |
| `test_nvs_manager.cpp` | Non-volatile storage (NVS) operations |
| `test_coredump.cpp` | Core dump functionality |
| `test_application_version.cpp` | Application version tracking |
| `test_common.cpp` | Common utilities and helpers |
| `test_helpers.cpp` | Helper functions for test infrastructure |

## Test Infrastructure

### Mock Classes

The test system uses mock classes to isolate functionality without requiring full system initialization:

- **`MockMessageDispatcher`**: Simulates message dispatch without SPI communication
- **`MockTransportProvider`**: Simulates transport layer without actual hardware

### Friend Classes

Friend classes allow access to private methods and members for testing:

- **`WiFiRequestHandlerTestFriend`**: Provides access to WiFi internals for testing

## Building the Tests

### Prerequisites

Before building the tests, create a `secrets.h` file in the `test/` directory containing WiFi credentials:

```bash
cp test/secrets.template test/secrets.h
# Edit test/secrets.h with your WiFi SSID and password
```

The `secrets.h` file should contain:

```c
#define TEST_WIFI_SSID "YourNetworkSSID"
#define TEST_WIFI_PASSWORD "YourNetworkPassword"
```

> **Important**: The `secrets.h` file is in `.gitignore` to prevent credentials from being committed.

### Build Commands

Build the test application using the `test/build.sh` script:

```bash
# Basic test build
cd test
./build.sh --build

# Full clean and rebuild
./build.sh --fullclean

# Build with debugging
./build.sh --develop --build

# Build and flash (using the serial interface)
./build.sh --build --flash
```

The build script in the test directory supports the same options as the main `build.sh` script. See [Building](Building.md) for complete option documentation.

### Test Build Output

The test build creates a test application with an interactive menu system for selecting and running individual tests:

```
build/
├── MeadowComms.bin       # Test application binary
├── MeadowComms.elf       # Test application with debug symbols
└── ...                   # Other build artifacts
```

## Running Tests

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

1. Build and flash the test application with heap tracing enabled:
   ```bash
   ./build.sh --heaptracing --build --flash
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

### Core Dump Testing

Tests related to core dump functionality verify that crash information is properly captured and can be analyzed. Enable core dumps with:

```bash
./build.sh --coredump --build
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

### Friend Class Testing

Example using `WiFiRequestHandlerTestFriend`:

```cpp
void test_wifi_private_method()
{
    WiFiRequestHandlerTestFriend handler;
    
    // Access private methods through friend class
    bool result = handler.PrivateHelperMethod();
    TEST_ASSERT_TRUE(result);
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

### Issue: Secrets File Missing

**Solution**: Create the secrets file:
```bash
cp test/secrets.template test/secrets.h
# Edit with your WiFi credentials
```

### Issue: Test Hangs or Never Completes

**Solution**: 
1. Check WiFi credentials in `secrets.h`
2. Increase test timeout in test file
3. Run with verbose logging:
   ```bash
   ./build.sh --develop --debug=WIFI --build
   ```

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

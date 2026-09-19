# Building the MeadowComms Application

The MeadowComms application is a FreeRTOS application built using the ESP-IDF framework. This document describes how to build the application for both development and production deployment.

## Prerequisites

### System Requirements

- **Operating System**: Linux, macOS, or Windows (with WSL2 recommended)
- **ESP-IDF Version**: 5.3 or later
- **Python Version**: 3.8 or later
- **C++ Standard**: C++20 compatible compiler

### Installation Methods

#### Option 1: Visual Studio Code Extension (Recommended)

The easiest way to install the ESP-IDF tools is using the [VS Code ESP-IDF Extension](https://marketplace.visualstudio.com/items?itemName=espressif.esp-idf-extension):

1. Install the extension from the VS Code Marketplace
2. The extension will guide you through tool installation
3. Ensure you select **Release 5.3** or later of the ESP-IDF
4. Allow the extension to configure your environment

#### Option 2: Docker

Build using the [Espressif Docker image](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-docker-image.html):

```bash
docker run --rm -v $PWD:/project -w /project espressif/idf:release-5.3 idf.py build
```

> **Note**: Docker deployment (flashing) is not supported on Windows or macOS due to serial port access limitations. Use native tools for deployment.

#### Option 3: Manual Installation

Follow the [Espressif Installation Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) for manual setup.

## Building the Application

### Quick Build

The simplest approach is to use the provided `build.sh` script:

```bash
./build.sh --build
```

This performs a standard release build of the application.

### Using the Build Script with Options

The `build.sh` script provides multiple options for different build scenarios:

```bash
./build.sh [OPTIONS]
```

#### Common Build Options

| Option | Description |
|--------|-------------|
| `-b`, `--build` | Build the application in release mode (default) |
| `-c`, `--clean` | Perform a clean build (removes old build artifacts) |
| `--fullclean` | Remove the entire build directory and rebuild |
| `-v`, `--verbose` | Display all executed commands (default) |
| `-s`, `--silent` | Execute commands silently (show only errors) |
| `--help` | Display help information |

#### Debug and Diagnostic Options

| Option | Description |
|--------|-------------|
| `--develop` | Enable development mode with debug code |
| `-tm`, `--tracemessages` | Enable message tracing |
| `-pl`, `--performancelogging` | Enable performance logging of method entry and exit times |

#### Subsystem-Specific Debug Options

| Option | Values | Description |
|--------|--------|-------------|
| `-d=*`, `--debug=*` | WIFI, SPI, BT, SYSTEM | Enable debug output for specific subsystems (comma-separated) |

Examples:
```bash
./build.sh --debug=WIFI,SPI        # Debug WiFi and SPI
./build.sh --debug=BT              # Debug Bluetooth
```

#### Other Options

| Option | Description |
|--------|-------------|
| `-f`, `--flash` | Flash the device after building |
| `-m`, `--monitor` | Monitor the device serial port after building |
| `--sbom` | Generate a Software Bill of Materials (SBOM) in SPDX format |
| `--dryrun` | Show what would be executed without running commands |

### Build Examples

#### Release Build
```bash
./build.sh --build
```

#### Development Build with Debug Output
```bash
./build.sh --develop --debug=WIFI --build
```

#### Full Clean and Rebuild
```bash
./build.sh --fullclean
```

#### Build, Flash, and Monitor
```bash
./build.sh --build --flash --monitor
```

#### Build with All Debugging Features
```bash
./build.sh --build --develop --tracemessages --performancelogging
```

### Direct ESP-IDF Commands

For more control, you can use the ESP-IDF tools directly:

```bash
# Build the application
idf.py build

# Clean build artifacts
idf.py clean

# Full clean (recommended if build fails)
idf.py fullclean

# Set build target
idf.py set-target esp32

# Configure the project interactively
idf.py menuconfig
```

> **Note**: If a build fails, try running `idf.py fullclean` before rebuilding, as the incremental clean process (`idf.py clean`) may not completely remove all build artifacts.

## Build Output

The build process creates the following directory structure:

```
build/
├── bootloader/           # Bootloader binary and build files
├── partition_table/      # Partition table files
├── MeadowComms.bin       # Application binary
├── MeadowComms.elf       # Executable with debug symbols
├── compile_commands.json # Compilation database (useful for IDE integration)
└── config/              # Configuration files
```

## Compilation Defines

The build system supports various compilation defines that can be set to enable/disable features. These are typically set via the `build.sh` script options shown above.

Common defines include:

- `DEBUG` - Enable general debug mode
- `WIFI_DEBUG` - Enable WiFi debug output
- `SPI_DEBUG` - Enable SPI debug output
- `BLUETOOTH_DEBUG` - Enable Bluetooth debug output
- `SYSTEM_DEBUG` - Enable system debug output
- `MESSAGES_DEBUG` - Enable message protocol debug output
- `TRACE_MESSAGES` - Enable detailed message tracing
- `DIAGNOSTICS` - Enable diagnostic features
- `PERFORMANCE_LOGGING` - Enable performance logging

## Memory Configuration

The ESP32 has limited memory. Key memory settings are configured in `sdkconfig` and `sdkconfig.defaults`:

- **Total SRAM**: ~320 KB available (shared between instruction cache, data, and heap)
- **PSRAM**: Optional external SPI RAM (not used in current implementation)
- **Partition Table**: Defined in `partitions.csv`

For memory-intensive operations, use the heap tracing tools documented in [Diagnostics](Diagnostics.md).

## Troubleshooting Build Issues

### Issue: Build Fails with Linking Errors

**Solution**: Perform a full clean:
```bash
idf.py fullclean
./build.sh --fullclean
```

### Issue: Cannot Find ESP-IDF

**Solution**: Ensure the `IDF_PATH` environment variable is set:
```bash
export IDF_PATH=/path/to/esp-idf
```

### Issue: Serial Port Not Found During Flashing

**Solution**: Check available ports:
```bash
# On macOS/Linux
ls /dev/tty.*

# On Windows
mode COM:
```

Then specify the port:
```bash
idf.py -p /dev/ttyUSB0 flash
```

### Issue: Out of Memory During Compilation

**Solution**: Reduce parallel build jobs:
```bash
idf.py -j 1 build
```

## Flashing the Device

Once built, the application must be flashed to the ESP32. See [Development Tools](Tools.md) for detailed flashing instructions.

## Next Steps

- [Running Unit Tests](../test/readme.md)
- [Development Tools and Flashing](Tools.md)
- [Diagnostics and Debugging](Diagnostics.md)

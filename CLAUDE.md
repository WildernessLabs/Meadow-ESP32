# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Meadow-ESP32** is ESP32 firmware (C++20/C11, ESP-IDF 5.3+, FreeRTOS) that provides WiFi, Bluetooth, POSIX networking, and file storage services to an external STM32 microcontroller via SPI and UART.

- **Target**: ESP32-PICO-D4
- **STM32→ESP32**: UART for control/state, SPI for request/response
- **Default branch**: `develop`

## Build Commands

```bash
./build.sh --build                      # Release build
./build.sh --develop                    # Build with debug symbols (DDEBUG=1)
./build.sh --build --flash --monitor    # Build, flash, and monitor
./build.sh --fullclean                  # Full rebuild (use when incremental fails)
./build.sh --help                       # All options

# Debug subsystems: WIFI, SPI, BT, SYSTEM
./build.sh --develop --debug=WIFI --build

# Direct idf.py
idf.py build
idf.py menuconfig
idf.py monitor
```

### Diagnostic Build Flags

| Flag | Effect |
|------|--------|
| `--develop` | `DEBUG=1` + debug symbols |
| `--tracemessages` | SPI message protocol tracing |

## Testing

Tests live in `components/F7Services/test/` and use the Unity framework. Run from a separate build in `test/`:

```bash
# From the root of the repository, once: settings the tests read from the device.
cp test/main/unittest.config.json.template test/fsimage/unittest.config.json
./mkfsimage.sh --flash          # Write the test settings to the board
                                # After a --fullclean: idf.py reconfigure in test/ first

cd test
./build.sh --build
./build.sh --flash
idf.py monitor                  # Interactive menu selects test
```

## Architecture

### Message Flow

```
STM32 (SPI request)
  → SpiTransportProvider        # Hardware SPI layer
  → MessageDispatcher           # Routes by message type (singleton)
      ├─ WiFiRequestHandler      # WiFi + POSIX socket requests
      ├─ BluetoothRequestHandler # BLE requests
      └─ SystemRequestHandler    # FileSystem, NVS, system state
  → STM32 (SPI response)
```

Communication protocol details: `Documentation/CommunicationsOverview.md`, `Documentation/SPIProtocolOverview.md`, `Documentation/MessageFormat.md`.

### Threading Model

1. **Main Task** — entry point, creates handlers, then self-deletes
2. **SPI Task** — receives/transmits SPI packets (stack: 8192 B)
3. **Dispatcher Task** — routes messages to handlers
4. **Handler Tasks** — WiFi, Bluetooth, System each run their own task
5. **ThreadPool** — additional workers for parallel processing

### Component Layout

All source is under `components/F7Services/`:

| Subdirectory | Responsibility |
|---|---|
| `Messaging/` | SPI transport, message dispatch, encoding, base classes |
| `WiFi/` | WiFi connection, AP scan, POSIX socket API |
| `Bluetooth/` | BLE manager, services, characteristics, descriptors |
| `System/` | File system (LittleFS/SPIFFS), NVS, CoreDump, Thread/ThreadPool |
| `Diagnostics/` | Debug utilities |
| `Exceptions/` | Custom exception types |
| `test/` | Unity tests + mock classes |

Top-level files: `Logging.*`, `Mapping.*`, `Globals.hpp`, `Utils.*`, `ProgressiveDelay.*`, `Gpio.hpp`, `Debug.hpp`.

### Flash Partitions

There are two tables and the build picks one. `partitions.csv` is the release layout,
used by `sdkconfig.defaults`. `partitions-develop.csv` is merged in by `--develop` and
by the test build; it gives up application space for a core dump partition large enough
to be useful.

`partitions.csv` (release):

| Name | Offset | Size |
|---|---|---|
| nvs | 0x9000 | 24 KB |
| phy_init | 0xf000 | 4 KB |
| factory | 0x10000 | 3.625 MB |
| coredump | 0x3b0000 | 64 KB |
| littlefs | 0x3c0000 | 256 KB |

`partitions-develop.csv` (`--develop` and the tests):

| Name | Offset | Size |
|---|---|---|
| nvs | 0x9000 | 24 KB |
| phy_init | 0xf000 | 4 KB |
| factory | 0x10000 | 3.125 MB |
| coredump | 0x330000 | 512 KB |
| littlefs | 0x3c0000 | 256 KB |

`littlefs` is at 0x3c0000 in both, so one filesystem image serves either build.
`mkfsimage.sh` reads the offset from whichever table `sdkconfig` names rather than
assuming one.

## Code Conventions

**Formatting**: Allman style, 4-space indentation, rules in `.clang-format`.

**Spelling**: UK English throughout (`ise` not `ize`).

**Docstrings**: Required on all namespaces, classes, functions, and variables.

**Naming**:
| Construct | Style |
|---|---|
| Classes, Functions, Namespaces | `PascalCase` |
| Constants | `UPPER_SNAKE_CASE` |
| Private member variables | `_camelCase` |
| Function parameters | `camelCase` |
| File names | `PascalCase.ext` |

**C++ specifics**:
- `nullptr` not `NULL`
- `return()` style for return statements
- `const` wherever appropriate
- Use whole words, not abbreviations
- Spaces around operators, after commas, after casts
- No `else if` chains — use a nested `else { if (...) { ... } }` instead. `clang-format` will not flag `else if`, so this has to be right when written.

## Key Build Notes

- `build.sh` injects git commit hash, branch, and build timestamp into `main/build_info.h` from `build_info.template`, then restores the template after build.
- SPI transaction hard limit: **4092 bytes** (hardware constraint).
- SDK config files: `sdkconfig.defaults` (release), `sdkconfig.develop` (merged on `--develop`), `sdkconfig` (generated, not committed).
- Managed dependencies (via `idf_component.yml`): `joltwallet/littlefs` v1.5.5. `espressif/cjson` ==1.7.19 is also declared but gated to `idf_version >=6.0`; on the 5.3+ toolchain this project actually builds with, that rule never matches, so JSON parsing uses ESP-IDF's bundled `json` component instead (cJSON 1.7.19, verified free of the known CVE).

## Documentation

`Documentation/` contains detailed guides:
- `Building.md` — build options
- `Testing.md` — test setup
- `CommunicationsOverview.md` — STM32↔ESP32 protocol
- `SPIProtocolOverview.md` — SPI low-level details
- `MessageFormat.md` — message structure
- `Diagnostics.md` — debugging techniques
- `Tools.md` — VS Code, Docker, OpenOCD, GDB setup

# Meadow-ESP32

[![Build](https://github.com/WildernessLabs/Meadow-ESP32/actions/workflows/build.yml/badge.svg)](https://github.com/WildernessLabs/Meadow-ESP32/actions/workflows/build.yml)

[![Unit tests](https://github.com/WildernessLabs/Meadow-ESP32/actions/workflows/unit-tests.yml/badge.svg)](https://github.com/WildernessLabs/Meadow-ESP32/actions/workflows/unit-tests.yml) Unit Tests (Emulated Only)

This repository contains the code that will run on the ESP32 to support communication and facilitate the provision of services to NuttX and Meadow.Core.

Services are provided to the STM32 through the SPI interface connected to the STM32 and the ESP32 chips.

## Quick Start

### Building

```bash
# Install ESP-IDF 5.3+ (via VS Code extension or manually)
./build.sh --build          # Build release version
./build.sh --develop        # Build with debug symbols
./build.sh --help           # See all options
```

See [Building the System](Documentation/Building.md) for detailed instructions.

### Running Tests

```bash
# From the root of the repository, once: settings the tests read from the device.
cp test/main/unittest.config.json.template test/fsimage/unittest.config.json
./mkfsimage.sh --flash          # Write the test settings to the board

cd test
./build.sh --build --flash --monitor
# Select test from interactive menu
```

See [Testing Guide](Documentation/Testing.md) for detailed information.

## Architecture

The system uses a message-based architecture with FreeRTOS tasking:

- **WiFi**: Connectivity and POSIX socket API
- **Bluetooth**: BLE service and characteristic management  
- **System**: File storage, NVS configuration, threading
- **Messaging**: SPI protocol and message dispatch

See [Project Structure](Documentation/ProjectStructure.md) for detailed component information.

## Documentation

### Getting Started

- [Building the System](Documentation/Building.md) - Comprehensive build instructions and options
- [Testing Guide](Documentation/Testing.md) - Unit test setup and execution
- [Project Structure](Documentation/ProjectStructure.md) - Component organization and architecture

### Technical Documentation

- [Message Format](Documentation/MessageFormat.md) - Message structure and protocol
- [Communication Process Overview](Documentation/CommunicationsOverview.md) - STM32-ESP32 communication
- [SPI Protocol Overview](Documentation/SPIProtocolOverview.md) - Low-level SPI protocol details
- [Event Processing](Documentation/EventProcessing.md) - Interrupt and event handling

### Development and Debugging

- [Development Tools](Documentation/Tools.md) - Tool setup and installation
- [Diagnostics](Documentation/Diagnostics.md) - Debugging and diagnostic output
- [Hints and Tips](Documentation/HintsAndTips.md) - Development tricks and patterns

### System Features

- [Configuration Values](Documentation/StoringConfiguration.md) - Non-volatile storage (NVS)
- [Mapping Values](Documentation/MappingValues.md) - Enum mapping between systems

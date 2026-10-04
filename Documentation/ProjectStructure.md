# Project Structure and Architecture

This document describes the organization of the MeadowComms project and the role of each major component.

## Directory Layout

```
Meadow-ESP32/
├── main/                        # Main application entry point
│   ├── AppMain.cpp             # Application startup code
│   ├── AppMain.hpp             # Application declarations
│   ├── CMakeLists.txt          # Build configuration for main component
│   └── idf_component.yml       # Component metadata
│
├── components/F7Services/      # Core services and messaging system
│   ├── Messaging/              # Message dispatch and SPI communication
│   │   ├── MessageDispatcher.cpp/.hpp      # Routes messages to subsystems
│   │   ├── SpiTransportProvider.cpp/.hpp   # Handles SPI hardware interface
│   │   ├── Esp32Messaging.cpp/.hpp         # ESP32-specific messaging
│   │   ├── TransportProviderBase.cpp/.hpp  # Abstract transport layer
│   │   ├── RequestHandlerBase.cpp/.hpp     # Base class for request handlers
│   │   ├── Encoders.cpp/.hpp               # Message encoding/decoding
│   │   └── SharedEnums.hpp                 # Shared constants and enums
│   │
│   ├── WiFi/                   # WiFi connectivity services
│   │   ├── WiFiRequestHandler.cpp/.hpp     # Handles WiFi requests
│   │   ├── AccessPoint.cpp                 # Access point scanning/connection
│   │   └── PosixAPIHandlers.cpp            # POSIX socket API handlers
│   │
│   ├── Bluetooth/              # Bluetooth LE services
│   │   ├── BluetoothManager.cpp/.hpp       # Manages BLE device
│   │   ├── BluetoothRequestHandler.cpp/.hpp # Handles BT requests
│   │   ├── BluetoothService.cpp/.hpp       # BLE service definition
│   │   ├── BluetoothCharacteristic.cpp/.hpp # BLE characteristic
│   │   ├── BluetoothAttribute.cpp/.hpp     # BLE attribute base
│   │   ├── BluetoothDescriptor.cpp/.hpp    # BLE descriptor
│   │   ├── BluetoothHandleManager.cpp/.hpp # Manages BLE handles
│   │   ├── BluetoothServiceCollection.cpp/.hpp
│   │   ├── BluetoothCharacteristicDeclaration.cpp/.hpp
│   │   ├── BluetoothCharacteristicValue.cpp/.hpp
│   │   ├── BluetoothComponentName.hpp      # Component naming
│   │   └── Uuid.cpp/.hpp                   # UUID handling
│   │
│   ├── System/                 # System-level services
│   │   ├── SystemRequestHandler.cpp/.hpp   # Handles system requests
│   │   ├── FileSystem.cpp/.hpp             # SPIFFS file operations
│   │   ├── NvsManager.cpp/.hpp             # Non-volatile storage (NVS)
│   │   ├── CoreDump.cpp/.hpp               # Core dump capture
│   │   ├── Thread.cpp/.hpp                 # Thread management
│   │   ├── ThreadPool.cpp/.hpp             # Thread pool implementation
│   │   └── build_info.template             # Build information template
│   │
│   ├── Diagnostics/            # Diagnostic and debugging support
│   │   └── (Debug utilities)
│   │
│   ├── Exceptions/             # Exception definitions
│   │   └── (Exception classes)
│   │
│   ├── test/                   # Unit tests
│   │   ├── CMakeLists.txt
│   │   ├── Readme.md
│   │   ├── Test*.cpp           # Individual test files
│   │   ├── TestHarness.cpp/.hpp
│   │   ├── TestAssertions.cpp/.hpp
│   │   ├── TestEncoders.cpp/.hpp
│   │   ├── MockMessageDispatcher.cpp/.hpp
│   │   └── MockTransportProvider.cpp/.hpp
│   │
│   ├── CMakeLists.txt          # Build configuration for F7Services
│   ├── idf_component.yml       # Component metadata
│   ├── Logging.cpp/.hpp        # Logging utilities
│   ├── Mapping.cpp/.hpp        # Enum mapping between systems
│   ├── Globals.hpp             # Global state and constants
│   ├── Debug.hpp               # Debug macros and utilities
│   ├── Utils.cpp/.hpp          # General utility functions
│   ├── ProgressiveDelay.cpp/.hpp # Retry backoff implementation
│   └── Gpio.hpp                # GPIO definitions
│
├── test/                        # Standalone test build system
│   ├── build.sh                # Test build script
│   ├── flash.sh                # Test flash script
│   ├── gdbinit                 # GDB initialization
│   ├── readme.md               # Test documentation
│   ├── main/                   # Test application
│   └── CMakeLists.txt
│
├── Documentation/              # Project documentation
│   ├── Building.md             # Building instructions
│   ├── Testing.md              # Testing guide
│   ├── CommunicationsOverview.md    # Communication protocol
│   ├── SPIProtocolOverview.md       # SPI protocol details
│   ├── MessageFormat.md        # Message structure
│   ├── Tools.md                # Development tools
│   ├── Diagnostics.md          # Debugging and diagnostics
│   ├── Hints and Tips.md       # Development tips
│   ├── EventProcessing.md      # Event system
│   ├── StoringConfiguration.md # NVS storage
│   ├── MappingValues.md        # Enum mapping
│   ├── SupportFiles/           # Documentation images
│   └── WaveDromFiles/          # Timing diagrams
│
├── CMakeLists.txt              # Top-level CMake configuration
├── build.sh                    # Main build script
├── sdkconfig                   # Current SDK configuration
├── sdkconfig.defaults          # Default SDK configuration
├── sdkconfig.develop           # Development configuration
├── partitions.csv              # Flash partition table
├── version.txt                 # Version information
└── README.md                   # Project overview
```

## Component Overview

### Core Components

#### main/
The main application component that serves as the entry point. Contains minimal code - primarily instantiates the `MessageDispatcher` and `SpiTransportProvider`, then exits. The actual work is done in the messaging and subsystem components.

**Key Files**:
- `AppMain.cpp`: Sets up the system and starts core services

#### components/F7Services/

The main services component containing all business logic and subsystems.

### Subsystem Components

#### Messaging/
Handles inter-component communication and SPI protocol.

**Responsibilities**:
- Decoding incoming SPI messages from the STM32
- Routing messages to the appropriate subsystem handler
- Encoding and sending responses back to the STM32
- Managing message acknowledgements and error handling

**Key Classes**:
- `MessageDispatcher`: Routes messages to handlers
- `SpiTransportProvider`: Low-level SPI communication
- `RequestHandlerBase`: Base class for all request handlers
- `Encoders`: Encode/decode message headers and payloads

#### WiFi/
Manages WiFi connectivity and POSIX socket API.

**Responsibilities**:
- WiFi network scanning
- WiFi connection management
- WiFi event handling
- POSIX socket API implementation (socket, connect, send, recv, etc.)

**Key Classes**:
- `WiFiRequestHandler`: Processes WiFi requests
- `AccessPoint`: Manages AP scanning and connection

#### Bluetooth/
Manages Bluetooth LE services and characteristics.

**Responsibilities**:
- BLE service creation and management
- BLE characteristic handling
- BLE descriptor management
- BLE event handling

**Key Classes**:
- `BluetoothManager`: Manages overall BLE device state
- `BluetoothRequestHandler`: Processes BLE requests
- `BluetoothService`: Represents a BLE service
- `BluetoothCharacteristic`: Represents a BLE characteristic
- `BluetoothAttribute`: Base class for BLE attributes

#### System/
Manages system-level services and resources.

**Responsibilities**:
- File system (SPIFFS) operations
- Non-volatile storage (NVS) management
- System configuration storage and retrieval
- Thread management and thread pool
- Core dump capture and analysis

**Key Classes**:
- `SystemRequestHandler`: Processes system requests
- `FileSystem`: SPIFFS file operations
- `NvsManager`: Non-volatile storage access
- `ThreadPool`: Manages worker threads
- `CoreDump`: Captures and manages core dumps

### Support Components

#### Logging/
Provides logging infrastructure using ESP-IDF's logging system.

#### Mapping/
Maps enums and constants between NuttX and ESP-IDF representations (e.g., socket options, error codes).

#### Globals/
Shared global state and constants.

#### Utils/
General utility functions used across subsystems.

## Message Flow Architecture

The system uses a message-based architecture for communication between STM32 and ESP32:

```
STM32
  ↓ (SPI Request)
SPI Interface
  ↓
MessageDispatcher
  ├─→ WiFi Handler
  ├─→ Bluetooth Handler
  └─→ System Handler
  ↑ (SPI Response)
SPI Interface
  ↑
STM32
```

## Threading Model

The system uses FreeRTOS threading:

1. **Main Task**: Entry point, creates message dispatcher
2. **SPI Task**: Handles incoming SPI packets
3. **Message Dispatcher Task**: Routes messages to handlers
4. **Subsystem Tasks**: WiFi, Bluetooth, System handlers run in queue tasks
5. **Thread Pool**: Additional worker threads for parallel processing

## Message Processing Pipeline

1. **Receive**: SPI task receives message from STM32
2. **Validate**: Check the message structure against the bytes received (`Encoders::PacketIsWellFormed`), then verify the CRC. The two are independent: the CRC shows transmission integrity only, so the structure checks never depend on it (see `MessageFormat.md`)
3. **Parse**: Extract header and payload
4. **Route**: Message dispatcher determines target subsystem
5. **Handle**: Appropriate handler processes the message
6. **Respond**: Handler queues response back to SPI
7. **Send**: SPI task transmits response to STM32

## Key Design Patterns

### Request-Response Pattern
Clients send requests and receive responses. Each request has a unique message ID for correlation.

### Event-Driven Architecture
Subsystems generate events (e.g., WiFi disconnected, Bluetooth notification) that are sent back to STM32.

### Handler Pattern
Each subsystem implements a request handler that processes its specific message types.

### Factory Pattern
Bluetooth services and characteristics are created through factory methods.

### Observer Pattern
WiFi and Bluetooth event handlers register callbacks for system events.

## Build System

The project uses CMake and ESP-IDF's build system:

- **CMakeLists.txt**: Defines components and dependencies
- **idf_component.yml**: Component metadata and version info
- **build.sh**: User-friendly build script with options
- **Makefile**: Legacy build support (ESP-IDF also supports this)

## Configuration and Customization

### Partition Table
`partitions.csv` defines the flash memory layout:
- Bootloader
- Partition table
- Non-volatile storage (NVS)
- PHY initialisation data
- Application (`factory`)
- Core dump
- File system (LittleFS)

There are no OTA update slots: the managed application performs updates and rewrites the `factory` partition in place.  See [Firmware Updates](FirmwareUpdate.md).

### SDK Configuration
- `sdkconfig.defaults`: Default settings
- `sdkconfig.develop`: Development-specific overrides
- `sdkconfig`: Current active configuration (generated)

## Testing Architecture

Tests follow the same component structure:

```
components/F7Services/test/
├── TestWiFi.cpp
├── TestBluetooth.cpp
├── TestFileSystem.cpp
├── TestSystem.cpp
├── MockMessageDispatcher   # Mock implementations
├── MockTransportProvider
└── test_common.*           # Test infrastructure
```

Tests use:
- **Unity Framework**: For assertions and test management
- **Mock Classes**: To isolate units under test
- **Friend Classes**: To access private methods

## Dependency Graph

```
main
  ↓
F7Services (all)
  ├─ Messaging (core)
  │   ├─ TransportProviderBase
  │   └─ RequestHandlerBase
  │
  ├─ WiFi (depends on Messaging)
  │
  ├─ Bluetooth (depends on Messaging)
  │
  └─ System (depends on Messaging)
      ├─ FileSystem
      ├─ NvsManager
      └─ ThreadPool
```

## Performance Considerations

1. **Memory**: Limited heap (~320 KB available after system overhead)
2. **SPI**: 4092-byte transaction limit (hardware constraint)
3. **Threading**: Careful thread management to avoid stack overflow
4. **Event Handling**: Asynchronous processing to prevent blocking

## Future Extensions

The modular architecture allows easy addition of new subsystems:

1. Implement `RequestHandlerBase`
2. Create message handler in `MessageDispatcher`
3. Add message definitions to protocol
4. Create corresponding test module

## Related Documentation

- [Communication Overview](CommunicationsOverview.md)
- [Message Format](MessageFormat.md)
- [Building the System](Building.md)
- [Running Tests](Testing.md)

# Meadow-ESP32

This repository contains the code that will run on the ESP32 that will support the communication and facilitate the provision of services to NuttX and Mono.

Services are provided to the STM32 through the SPI interface connected to the STM32 and the ESP32 chips.

Additional documentation is available for the following items (click on the header to follow the link to the appropriate document).

## [Message Format](Documentation/MessageFormat.md)

Requests, responses and interrupts transfer data between the tw chips through a series of messages.  The maximum amount of data that can be transferred is restricted to 4,000 bytes due to a hardware limit on the ESP32.

## [Communication Process Overview](Documentation/CommunicationsOverview.md)

Communication between the two microcontrollers is achieved using a single SPI bus and two GPIO lines to signal system readiness.

## [ESP32 Development Tools](Documentation/Tools.md)

The ESP32 runs FreeRTOS and the ESP-IDF to provide a multitasking environment that can communicate with the various hardware subsystems.  The development tools document provides information about the setup of the tools for development and building the ESP32 software.

Documentation for ESP-IDF is available through the [Espressif documentation site](https://github.com/WildernessLabs/Meadow-ESP32).

ESP-IDF repository containing source code and examples is available on [GitHub](https://github.com/espressif/esp-idf).

## [File Generator](Documentation/FileGenerator.md)

Three languages are used to facilitate communication between the various subsystems, namely, C#, C and C++.

* C# is used in Meadow.Core
* NuttX is developed using C
* Code on the ESP32 uses both C and C++

The `FileGenerator` application uses two metadata files to generate message definitions, encoders and constants for all three languages.  This ensures consistency between the three environments.

## [Diagnostics](Documentation/Diagnostics.md)

At the basic level, the ESP32 provides serial output to facilitate basic `printf` debugging.

Heap debugging tools are also available along with deferred debug output through application tracing.

## [Configuration Values](Documentation/StoringConfiguration.md)

The ESP32 allows the storage of data that can survive resets and power loss.  The Non-Volatile Storage (NVS) is defined in the partition table at the time the chip is flashed.  It is recommended that any data stored should be small discrete values as the system is optimised for these types of values.  Strings and BLOBs can be stored but it is not recommended.

## [Mapping Values](Documentation/MappingValues.md)

The `SharedEnums.*` files cater for the consistent use of application defined values between the various components of the system but there are some values that are defined in the SDKs on the various systems (Nuttx and ESP-IDF).  Values such as the error codes, socket options values are examples of these types of values.  The `Mapping` class facilitates these mappings.

## [Hints and Tips](Documentation/HintsAndTips.md)

A number of hints and tips for development and debugging have been recorded over time in order to assist new development.

## Getting Started - Developer Edition

If you're just getting started developing for the ESP32, [start with this page](Documentation/DeveloperGettingStarted.md)
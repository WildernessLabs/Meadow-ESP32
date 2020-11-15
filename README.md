# Meadow-ESP32

This repository contains the code that will run on the ESP32 that will support the communication and facilitate the provision of services to NuttX and Mono.

Services are provided to the STM32 through the SPI interface connected to the STM32 and the ESP32 chips.

Additional documentation is availble for the following items (click on the header to follow the link to the appropriate document).

## [Message Format](Documentation/MessageFormat.md)

Requests, responses and interrupts transfer data between the tw chips through a series of messages.  The maximum amount of data that can be transferred is restricted to 4,000 bytes due to a hardware limit on the ESP32.

## [Communication Process Overview](Documentation/CommunicationProcess.md)

Communication between the two microcontrollers is achieved using a single SPI bus and two GPIO lines to signal system readiness.

## [ESP32 Development Tools](Development/Tools.md)

The ESP32 runs FreeRTOS and the ESP-IDF to provide a multitasking environment that can communicate with the various hardware subsystems.  The development tools document provides information about the setup of the tools for development and building the ESP32 software.

## [File Generator](Documentation/FileGenerator.md)

Three languages are used to facilitate communication between the various subsystems, namely, C#, C and C++.

* C# is used in Meadow.Core
* NuttX is developed using C
* Code on the ESP32 uses both C and C++

The `FileGenerator` application uses two metadata files to generate message definitions, encoders and constants for all three languages.  This ensures consistency between the three envoronments.

## [Diagnostics](Documentation/Diagnostics.md)

At the basic level, the ESP32 provides serial output to facilitate basic `printf` debugging.

Heap debugging tools are also available along with deferred debug output through application tracing.
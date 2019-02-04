# Tests for the Meadow Board

The code in these directories contain various tests for the ESP32 and STM32 communication code along with some hardware validation tests.

## STM32toESP32CommsTest

This folder contains STM32 code to test programming the ESP32 using the STM32.

## STM32toESP32GPIOConnectionTest

STM32 application used to verify the connections (GPIO) between the STM32 and the ESP32.  The STM32 toggles the GPIO and the ESP code responds by toggling the pin connected to the LED on the debug header.

## ESP32BatteryTest

ESP32 application that will monitor the battery voltage and output the readings on the serial port through GPIO23.

## MeadowAntennaTest

ESP32 application to check to see if the antenna can be selected using the GPIO pins attached to the ESP32.

## UARTTest

This folder contains the .NET code used to test the ESP32 application.  The initial version of the application communicates using serial and can be run on a PC.
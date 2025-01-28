# Unit Tests

These file contain tests for the following subsystems running on the ESP32:

* Bluetooth
* WiFi
* File system
* Helpers

As previously noted, the helper "tests" provide functionality to restart the board and turn on heap tracing to host.

## Supporting Classes

The unit tests are run through two main methods:

* Access to the methods in the classes under test
* Using the messaging system used by the STM32 and ESP to communicate with each other

### Friend Classes

Friend classes allow access to the private functionality of the class under test.  This can be useful for functionality such as *Scan for Access Points* where the full communication protocol is not required.

### Mock Communication

These classes simulate the core message passing functionality of communication system without requiring the actual SPI communication system to be active.
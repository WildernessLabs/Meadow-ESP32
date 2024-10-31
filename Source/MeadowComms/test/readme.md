# Running Unit Tests

The unit tests are run by the code in the *main/unit_tests.c* application code.  Examples on how to run one or more tests are left as commented code in the source code.

## Individual Tests

The individual component tests are located in the *components/F7Services/test* directory.

## Building the Tests

The unit test application requires a *secrets.h* file containing a number of definitions for items like the SSID and password for the access point.  This file is ignored (from a git perspective) to keep developer secrets from being added to GitHub.  The file *secrets.template* should be copied to *secrets.h* and the relevant macros edited before the system is built.

The tests can be built using the *test/build.sh* script using the same command line arguments as the main development build script.

The firmware is written using the *test/flash.sh* script or by using the *idf.py flash* command.

## Running the Tests

The tests are run by connecting a serial console to the ESP32 chip or using the command *idf.py monitor*.

# Running Unit Tests

The unit tests are run by the code in the *main/unit_tests.c* application code.  Examples on how to run one or more tests are left as commented code in the source code.

## Individual Tests

The individual component tests are located in the [*components/F7Services/test*](../components/F7Services/test/Readme.md) directory.

## Building the Tests

The unit test application requires a *secrets.h* file containing a number of definitions for items like the SSID and password for the access point.  This file is ignored (from a git perspective) to keep developer secrets from being added to GitHub.  The file *secrets.template* should be copied to *secrets.h* and the relevant macros edited before the system is built.

The tests can be built using the *test/build.sh* script using the same command line arguments as the main development build script.

The firmware is written using the *test/flash.sh* script or by using the *idf.py flash* command.

## Running the Tests

The tests are run by connecting a serial console to the ESP32 chip or using the command *idf.py monitor*.  This will restart the ESP32 and show the following:

```
#### Starting interactive test menu #####

Press ENTER to see the list of tests.
```

Pressing the enter key will show the list of available tests.  Tests can be run by entering the number of the test to be run.

### Helper "Tests"

A number of the tests are not tests, instead they provide some helper functionality.  The two main helpers are:

* Restart the board
* Enable heap tracing to host

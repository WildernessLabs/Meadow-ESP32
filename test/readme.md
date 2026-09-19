# Running Unit Tests

The unit tests are run by the code in the *main/unit_tests.c* application code.  Examples on how to run one or more tests are left as commented code in the source code.

## Individual Tests

The individual component tests are located in the [*components/F7Services/test*](../components/F7Services/test/Readme.md) directory.

## Building the Tests

The unit test application reads its settings from *unittest.config.json*, a file written to the board's file system by *mkfsimage.sh* rather than compiled into the application.  Changing a network or a server address therefore needs no rebuild.  The file is ignored (from a git perspective) to keep developer secrets from being added to GitHub; the template it is copied from, *test/main/unittest.config.json.template*, is in source control.  Copy the template to *test/fsimage/unittest.config.json*, edit it, and write it to the board with `./mkfsimage.sh --flash`.

The tests can be built using the *test/build.sh* script using the same command line arguments as the main development build script.

The tests always build with the development configuration, which the application only picks up when *build.sh* is given `--develop`.  It gives the core dump partition 512 KB and puts the RAM contents in the dump, so the core dump tests read back a full sized dump, and it turns on heap poisoning and the assertions, so that a test that corrupts the heap is caught where it happens.  See [Testing](../Documentation/Testing.md) for how the configuration files fit together.

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

## Heap Tracing

The ESP IDF provides two methods of heap tracing:

* [Host based](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/heap_debug.html#host-based-mode)
* [Device Based](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/heap_debug.html#standalone-mode)

Heap tracing to host is currently supported in the unit tests.

Some of the classes can also be analysed using *valgrind*.  Documentation for this analysis can be found in [valgrind desktop application](../Desktop/Readme.md) directory.

### Running Heap Tracking to Host

The following steps should be taken in order to turn on and analyse host based heap tracing:

* Build and flash the unit test application as normal.
* Open a terminal window and run the command *idf.py monitor*
* Open a second terminal window and run the command *idf.py openocd*
* Open a third terminal window, navigate to the unit test application directory and run the command *xtensa-esp32-elf-gdb -x gdbinit build/MeadowComms.elf*
* Execute the helper *Turn on Heap Tracing Data* by selecting it from the test menu
* Now run the selected test

The *xtensa-esp32-elf-gdb* command will stop at the end of the test and drop out to the GDB command prompt.  At this point the three terminal sessions can exit the various commands:

* *xtensa-esp32-elf-gdb* - use the *quit* command
* *idf.py monitor* - use the key sequence *Ctrl-]*
* *openocd* - use the key sequence *Ctrl-C*

The heap traces can now be analysed with the command *$IDF_PATH/tools/esp_app_trace/sysviewtrace_proc.py -p -b build/MeadowComms.elf heap_log0.svdat > memory.log*.  The *memory.log* file will contain the resulting logs.

# MeadowComms Build and Deployment

The MeadowComms application runs on the ESP32 as a native application built using the ESP-IDF.

## Docker Image

The application can be built using the Docker image supplied by Espressif.  Documentation on the Docker image can be found [on the Espressif web site](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-docker-image.html).

The system can be built by executing the following command:

`docker run --rm -v $PWD:/project -w /project espressif/idf:release-5.3 idf.py build`

> Note: The build process can sometimes fail as the clean process (`idf.py clean`) does not always fully clean the previous build.  Execute the command `rm -R build` if the build fails.

It is currently not possible to deploy the system using the Docker image on Windows or Mac as the serial ports are not passed through to the Docker image.  It is necessary to use the native tools to deploy the application.

## Native Tools

### Installation Using VS Code

The native tools are probably best installed using the [VSCode extension](https://marketplace.visualstudio.com/items?itemName=espressif.esp-idf-extension).  This extension can be used to check the installation prerequisites and to install the correct tools and SDK.  Ensure that `Release 5.3` or later is selected when deploying the tools.

### Manual Installation

The installation guide for a manual installation can be found in the Espressif [installation documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html).

### Build

Comprehensive build instructions are available in the [Building the System](Building.md) documentation. This includes details about all available build options, debug configurations, and troubleshooting.

For quick reference, it is recommended that the system should be built using the `bash` script `build.sh`:

```bash
./build.sh --build          # Release build
./build.sh --develop        # Development build with debug symbols
./build.sh --help           # Display all available options
```

See [Building the System](Building.md) for complete details.

## Unit Testing

The project includes comprehensive unit tests for all subsystems. For complete testing instructions, including test setup, execution, and advanced features like heap tracing, see the [Testing Guide](Testing.md).

Quick test build:

```bash
cd test
./build.sh --build --flash --monitor
# Follow the interactive test menu
```

The application can be deployed using either:

* JTAG
* Serial port

Both of these ports can be accessed through the [ESP-Prog board](https://github.com/espressif/esp-iot-solution/blob/master/documents/evaluation_boards/ESP-Prog_guide_en.md).

#### JTAG

JTAG communication takes place over the 10 pin headers on the ESP-Prog and the Meadow development board.  The ESP-Prog and the Meadow development boards are connected using a standard 10 pin (2 x 5) 0.1" pitch IDC cable

Deployment using the JTAG interface is available through the `flash.sh` script.

Note that this method of deployment is faster than using the serial interface.

#### Serial

Serial deployment is possible using the 6-pin headers on the ESP-Prog board.  The connection between the ESP-Prog and the pogo board varies between the two revisions of the board.

Serial deployment on MacOS, Big Sur, requires version 3.5 of `pyserial` in order to function correctly.

Most of the Meadow development boards can accept a standard 6-pin IDC cable direct to the ESP-Prog serial interface.

Some of the development boards have the serial R<sub>x</sub> and T<sub>x</sub> pins crossed.  In this case it is necessary to use a modified 6-pin serial cable in order to establish communication with the host computer.  If a manual connection is made then use the diagram on the [Communication Interface](https://github.com/espressif/esp-iot-solution/blob/master/documents/evaluation_boards/ESP-Prog_guide_en.md#322-communication-interface) page as a guide.  Make a 1-1 connection between the pins on the ESP-Prog and the pogo board **except** pins 3 and 5.  These should be crossed, pin 3 on the ESP-Prog to pin 5 on the pogo board.

The ESP-Prog presents two serial ports to the host computer.  On a Mac these look something like `/dev/tty.serial14400` and `/dev/tty.serial14401`.  Experience has shown that the higher port number is connected to the serial interface through to the pogo board.

The serial connection can be checked by connecting a serial application the the serial port.  An unprogrammed ESP32 microcontroller will start and 900 milliseconds later the watchdog timer will expire and the chip will reset.  This repeats continuously. If a good connection has been made between the two boards then a repeating series of messages will appear on the connected serial application.

Once the correct port has been identified the `idf.py flash` command can be used to program the chip.  The full command required to flash the bootloader, partition table and the application is displayed at the end of the build process (`idf.py build`).

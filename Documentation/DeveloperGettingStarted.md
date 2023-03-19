# Getting Started with ESP32 Development

This document is intended to get you started developing Meadow ESP32 code.

## Hardware

Development hardware used:
- Macbook running MacOS Catalina (10.15.6)
- Meadow F7 running Beta 4
- V1 pogo bed

## Software

Described in-depth in the [tools readme](Tools.md), but the summary:
- [ESP IDF tool chain](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#get-started-get-esp-idf)
- Python 3.9
- 
- VS Code v 1.39
- VS Code Espressif IDF Extension


## Config

I had to add the following to ~/.bash_profile

```
alias python=python3
alias get_idf='. $HOME/esp/esp-idf/export.sh'
```

Determine serial port.  Run this twice, once without and once with the programmer plugged in to get the port name

```
$ ls /dev/cu.*
/dev/cu.Bluetooth-Incoming-Port	/dev/cu.usbserial-14300		/dev/cu.usbserial-14301
```

Test comms.  Second device seems to be mine;

```
$ screen /dev/cu.usbserial-14301 115200
...
rst:0x10 (RTCWDT_RTC_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
flash read err, 1000
ets_main.c 371 
ets Jun  8 2016 00:22:57
...
```

To detach `screen` from the port use `<ctrl-A>.` then close with `<ctrl-a>k`




I (295) spi_flash: detected chip: gd
I (298) spi_flash: flash io: dio
W (302) spi_flash: Detected size(4096k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (316) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
Hello world!
This is esp32 chip with 2 CPU core(s), WiFi/BT/BLE, silicon revision 1, 2MB embedded flash
Minimum free heap size: 295916 bytes

## Pushing code to the Meadow

- Run VSCode
- Open Workspace
- Open Terminal
- $ get_idf
- $ idf.py build flash monitor

To exit the `monitor` use `<ctrl-]>`

## Building New STM<-->ESP Comms

If you need to add a message for comms between the STM and the ESP, this is the general roadmap.

0. Do *not* modify the `Encoders` files.
1. Modify the `./Source/Metadata/Messages.json` and `./Source/Metadata/SharedEnums.json` to represent the data you'll pass and the messages used to pass them.
2. Add a couple environment variables to define where your source files are:
```
export EspcpSourceDirectory=/Users/christophertacke/repos/wilderness/Meadow/Meadow.OS/nuttx/configs/stm32f777zit6-meadow/src/espcp
export MeadowCoreSourceDirectory=/Users/christophertacke/repos/wilderness/Meadow/Meadow.Core/source/Meadow.Core/Devices/Esp32Coprocessor
```
3. Build the `FileGenerator`
> NOTE: This requires the .NET 5.0 SDK be installed
```
$ cd ./Meadow-ESP32/Source/FileGenerator/
$ dotnet build
```
4. Run the File Generator
```
$ dotnet bin/Debug/netcoreapp5.0/FileGenerator.dll
```

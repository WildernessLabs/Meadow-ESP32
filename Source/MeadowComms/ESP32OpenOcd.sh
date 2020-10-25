#!/bin/bash
$IDF_TOOLS_PATH/tools/openocd-esp32/v0.10.0-esp32-20191114/openocd-esp32/bin/openocd -s share/openocd/scripts -f interface/ftdi/esp32_devkitj_v1.cfg -f board/esp-wroom-32.cfg
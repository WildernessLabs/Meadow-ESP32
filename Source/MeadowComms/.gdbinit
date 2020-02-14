target remote :3333
mon reset halt
mon program_esp32 /Users/markstevens/SoftwareDevelopment/WildernessLabs/Meadow-ESP32/Source/MeadowComms/build/MeadowComms.bin 0x10000 verify 
mon esp32 appimage_offset 0x10000
mon reset halt
flushregs
set remote hardware-watchpoint-limit 2
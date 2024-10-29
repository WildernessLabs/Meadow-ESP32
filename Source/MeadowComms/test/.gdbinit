define hook-quit
    set confirm off
end

set style address foreground yellow

# target remote :3333 

# mon reset halt

#mon esp32 appimage_offset 0x10000
#mon reset halt

flushregs
set remote hardware-watchpoint-limit 2
set listsize 30

# b test_wifi.cpp:189

# thb app_main
c
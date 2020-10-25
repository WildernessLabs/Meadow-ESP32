source [find interface/ftdi/esp32_devkitj_v1.cfg]
source [find board/esp-wroom-32.cfg]

init
reset init
halt

set image build/MeadowComms.bin 

program_esp $image 0x10000 verify 

shutdown

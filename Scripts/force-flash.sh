#!/bin/bash
cd /project
docker run --privileged -v /dev/bus/usb:/dev/bus/usb -v /mnt/c/Users/lolibai/Documents/INVERITA/DWM3001C-starter-firmware:/project uberi/qorvo-nrf52833-board /usr/local/JLink_Linux_V792n_x86_64/JLinkExe -device NRF52833_XXAA -if SWD -speed 4000 -autoconnect 1 -CommandFile /project/Scripts/jlink-flash.txt

#!/bin/bash

echo "Compiling the overlay from .dts to .dtbo"

dtc -O dtb -o SAMPLER-GPIO-00A0.dtbo -b 0 -@ SAMPLER-GPIO.dts

cp SAMPLER-GPIO-00A0.dtbo /lib/firmware
echo -4 > /sys/devices/platform/bone_capemgr/slots
echo 'SAMPLER-GPIO' > /sys/devices/platform/bone_capemgr/slots
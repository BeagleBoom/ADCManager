#!/bin/bash

echo "Compiling the overlay from .dts to .dtbo"

dtc -O dtb -o SAMPLER-GPIO-00A0.dtbo -b 0 -@ SAMPLER-GPIO.dts

cp SAMPLER-GPIO-00A0.dtbo /lib/firmware
cd /usr/src/bb.org-overlays/
./install.sh
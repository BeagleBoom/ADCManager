//
// Created by Friedemann Stoffregen on 07.06.17.
//


#include <stdint.h>


uint32_t reverseBits(uint32_t num)
{
    uint32_t  NO_OF_BITS = sizeof(num) * 8;
    uint32_t reverse_num = 0;
    int i;
    for (i = 0; i < NO_OF_BITS; i++)
    {
        if((num & (1 << i)))
            reverse_num |= 1 << ((NO_OF_BITS - 1) - i);
    }
    return reverse_num;
}


uint32_t sendSPICommand(uint32_t command) {
    command = reverseBits(command);
    uint32_t __R30 = 0x0;
    uint32_t data = 0x00000000;          //  Initialize data.
    __R30 = __R30 & 0xFFFFFFDF; //  Chip select to LOW P9.27
    __R30 = __R30 & 0xFFFFFFFB; //  Clock to LOW   P9.30

    for (int i = 0; i <= 16; i++) {
        uint32_t bit = (command & 0x80000000) >> 30;
        command = command << 1;
        __R30 = __R30 & (0xFFFFFFFD);
        __R30 = __R30 | bit;
        __R30 = __R30 | 0x00000004; //  Clock goes high; bit set on MOSI.

        data = data << 1;           // Shift left; insert 0 at lsb.
        __R30 = __R30 & 0xFFFFFFFB; //  Clock to LOW   P9.30
    }
    __R30 = __R30 | 1 << 5; //  Chip select to HIGH

    return data;
}


int main() {
    // Set ADC0 and ADC1 to ADC-Mode.
    uint32_t pinSetup = 0b0010000000000011;
    sendSPICommand(pinSetup);

    // Setup ADC-Mode
    uint32_t adcMode = 0b0001001000000011;
    sendSPICommand(adcMode);
}
// S.211 Tabelle
//
// pru0adc.c
//  Attempt to duplicate Derek Molloy's
//  SPI ADC read program in C from assembly.
//  Chip Select:  P9.27 pr1_pru0_pru_r30_5 // ändern: P8.20 pr1_pru1_pru_r30_13 0xFFFFDFFF
//  MOSI:         P9.29 pr1_pru0_pru_r30_1 // ändern: P8.21 pr1_pru1_pru_r30_12
//  MISO:         P9.28 pr1_pru0_pru_r31_3 // ändern: P8.27 pr1_pru1_pru_r31_8
//  SPI CLK:      P9.30 pr1_pru0_pru_r30_2 // ändern: P8.28 pr1_pru1_pru_r30_10
//  Sample Clock: P8.46 pr1_pru1_pru_r30_1 // P8.46 pr1_pru1_pru_r30_1 (testing only)
//  Copyright (C) 2016  Gregory Raven
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "resource_table_1.h"
#include <am335x/pru_cfg.h>
#include <am335x/pru_intc.h>
#include <pru_rpmsg.h>
#include <rsc_types.h>
#include <stdint.h>
#include <stdio.h>

// Define remoteproc related variables.
#define HOST_INT ((uint32_t)1 << 30)

//  The PRU-ICSS system events used for RPMsg are defined in the Linux device
//  tree.
//  PRU0 uses system event 16 (to ARM) and 17 (from ARM)
//  PRU1 uses system event 18 (to ARM) and 19 (from ARM)
#define TO_ARM_HOST 18
#define FROM_ARM_HOST 19

//  Using the name 'rpmsg-pru' will probe the rpmsg_pru driver found
//  at linux-x.y.x/drivers/rpmsg_pru.c
#define CHAN_NAME "rpmsg-pru"
#define CHAN_DESC "Channel 30"
#define CHAN_PORT 30
#define PULSEWIDTH 900
#define CLK_HIGH __R30 = __R30 | (1 << 10)
#define CLK_LOW __R30 = __R30 & ~(1 << 10)
#define CS_HIGH __R30 = __R30 | (1 << 13) //  Chip select to HIGH
#define CS_LOW __R30 = __R30 & ~(1 << 13) //  Chip select to LOW
//  Used to make sure the Linux drivers are ready for RPMsg communication
//  Found at linux-x.y.z/include/uapi/linux/virtio_config.h
#define VIRTIO_CONFIG_S_DRIVER_OK 4

//  Buffer used for PRU to ARM communication.
int16_t payload[7];

#define PRU_SHAREDMEM 0x00010000
#define CMD_NOP ((uint32_t) 0x00000000)
#define CMD_RESET ((uint32_t) 0b0111100000000000)

volatile register uint32_t __R30;
volatile register uint32_t __R31;
uint32_t spiCommand;

short sendSPICommand(char command) {
    short data = 0x00;          //  Initialize data.
    for (int i = 7; i >= 0; i--) {
        int bit = (command & (1 << i)) >> i;

        if (bit) {
            __R30 = __R30 | (bit << 12);
        } else {
            __R30 = __R30 & 0xEFFF;
        }

        __delay_cycles(PULSEWIDTH);
        CLK_LOW;
        __delay_cycles(PULSEWIDTH); //  Delay to allow settling.
        CLK_HIGH;
        __delay_cycles(PULSEWIDTH);
        data = data << 1;           // Shift left; insert 0 at lsb.
        if (__R31 & (1 << 8)) //  Probe MISO data from ADC.
            data = data | 1; // might be wrong! ReCheck!
        else
            data = data & 0xFFFE; // might also be wrong...
    }
    return data;
}

void sendWord(char d1, char d2) {
    CLK_HIGH;
    __delay_cycles(PULSEWIDTH);
    CS_LOW;
    __delay_cycles(PULSEWIDTH);

    sendSPICommand(d1);
    __delay_cycles(PULSEWIDTH);

    sendSPICommand(d2);

    CS_HIGH;
    __delay_cycles(PULSEWIDTH);
}

uint16_t sendReceiveWord(char d1, char d2) {
    CLK_HIGH;
    __delay_cycles(PULSEWIDTH);
    CS_LOW;
    __delay_cycles(PULSEWIDTH);

    short r1 = sendSPICommand(d1);
    __delay_cycles(PULSEWIDTH);
    short r2 = sendSPICommand(d2);
    CS_HIGH;
    __delay_cycles(PULSEWIDTH);
    // TODO: WHY SHIFTING RIGHT BY ONE?
    return (((r1 << 8) & 0xFF00) | (r2 & 0x00FF))  >> 1;
}

void initADC() {
    sendWord(0x05, 0xAC); // Reset Chip
    sendWord(0x5a, 0x00); // Internal reference always on
    sendWord(0x18, 0x10); // set ADC input to 2xV_ref (10V)
    sendWord(0x20, 0x3F); // configure port 2, 3 as ADC
}

int main(void) {
    struct pru_rpmsg_transport transport;
    uint16_t src, dst, len;
    volatile uint8_t *status;
    //  1.  Enable OCP Master Port
    CT_CFG.SYSCFG_bit.STANDBY_INIT = 0;
    //  Clear the status of PRU-ICSS system event that the ARM will use to 'kick'
    //  us.
    CT_INTC.SICR_bit.STS_CLR_IDX = FROM_ARM_HOST;

    //  Make sure the drivers are ready for RPMsg communication:
    status = &resourceTable.rpmsg_vdev.status;
    while (!(*status & VIRTIO_CONFIG_S_DRIVER_OK));

    //  Initialize pru_virtqueue corresponding to vring0 (PRU to ARM Host
    //  direction).
    pru_rpmsg_init(&transport, &resourceTable.rpmsg_vring0,
                   &resourceTable.rpmsg_vring1, TO_ARM_HOST, FROM_ARM_HOST);

    // Create the RPMsg channel between the PRU and the ARM user space using the
    // transport structure.
    while (pru_rpmsg_channel(RPMSG_NS_CREATE, &transport, CHAN_NAME, CHAN_DESC,
                             CHAN_PORT) != PRU_RPMSG_SUCCESS);
    //  The above code should cause an RPMsg character to device to appear in the
    //  directory /dev.
    //  The following is a test loop.  Comment this out for normal operation.

    //  This section of code blocks until a message is received from ARM.
    while (pru_rpmsg_receive(&transport, &src, &dst, payload, &len) !=
           PRU_RPMSG_SUCCESS) {
    }


    //  2.  Initialization
    //  The data out line is connected to R30 bit 1.
    __R30 = 0x00000000;         //  Clear the output pin.
    //  The sample clock is located at shared memory address 0x00010000.
    //  Need a pointer for this address.  This is found in the linker file.
    //  The address 0x0001_000 is PRU_SHAREDMEM.
    uint32_t *clockPointer = (uint32_t *) 0x00010000;

    CS_HIGH;  // Initialize chip select HIGH.
    __delay_cycles(100000000); //  Allow chip to stabilize.
    //  3.  SPI Data capture loop.  This captures numSamples data samples from the
    //  ADC.
    uint16_t dataCounter = 0; // Used to load data transmission buffer payloadOut;

    //  The following is a hack to solve a problem with the loop running
    // while(!(*clockPointer)){__delay_cycles(5);}  //  Hold until the Master
    // clock from PRU1 goes high.

    /* while (!(*clockPointer == 7)) {
        __delay_cycles(5);
    }*/ //  Hold until the Master clock from PRU1 goes high.



    initADC();

    while (1) {
        // Setup PIN 6,7 as GPIO in
        sendWord(0x50, 0xC0);
        sendWord(0x00, 0x00); // Nop it
        sendWord(0x54, 0xC0);
        // receive gpios
        uint16_t gpio_in = sendReceiveWord(0x00, 0x00);

        // configure ADC sequence to 0, 1, 2, 3, 4, 5 and repeat
        sendWord(0x12, 0x3F);
        //sendWord(0x00, 0x00); // send NOP

        uint16_t adc0 = 0x0000;//sendReceiveWord(0x00, 0x00);
        uint16_t adc1 = 0x1000; //sendReceiveWord(0x00, 0x00);
        uint16_t adc2 = sendReceiveWord(0x00, 0x00);
        uint16_t adc3 = sendReceiveWord(0x00, 0x00);
        uint16_t adc4 = 0x4000; //sendReceiveWord(0x00, 0x00);
        uint16_t adc5 = 0x5000; //sendReceiveWord(0x00, 0x00);
        sendWord(0x00, 0x00); // send NOP;

        payload[0] = gpio_in | 0x8000; // set first bit to "1" so we can see whether it's an GPIO or ADC value

        if (adc0 & 0x8000) {
            adc0 = 0xF0F0;
        }
        if (adc1 & 0x8000) {
            adc1 = 0xF0F1;
        }
        if(adc2 & 0x8000){
            adc2 = 0xF0F2;
        }
        if(adc3 & 0x8000){
            adc3 = 0xF0F3;
        }
        if(adc4 & 0x8000){
            adc4 = 0xF0F4;
        }
        if(adc5 & 0x8000){
            adc5 = 0xF0F5;
        }

        payload[1] = adc0;
        payload[2] = adc1;
        payload[3] = adc2;
        payload[4] = adc3;
        payload[5] = adc4;
        payload[6] = adc5;


        pru_rpmsg_send(&transport, dst, src, payload, 14);
        while (pru_rpmsg_receive(&transport, &src, &dst, payload, &len) !=
               PRU_RPMSG_SUCCESS) {
        }
    } //  End data acquisition loop.

    //   __R31 = 35;                      // PRUEVENT_0 on PRU0_R31_VEC_VALID
    __halt(); // halt the PRU
}

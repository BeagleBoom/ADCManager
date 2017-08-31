// create_sinusoidal_pcm_file.c
// Copyright 2016 Greg
// Generate sinusoidal PCM data and write to a named pipe.
// Play the data from the pipe using ALSA aplay via a
// fork and execvp.
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

#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>

#define REFERENCE_VCC 5


int16_t getTone(int16_t value) {
    float_t oneToneSize = 0xfff / (REFERENCE_VCC * 12);
    return (int16_t) (0.5 + (value / oneToneSize));
}

void publish(char name[], int16_t value) {
    printf("%s = %i\n", name, value);
}

int main(int argc, char **argv) {
    int pru_data, pru_clock; // file descriptors
    //  Open a file in write mode.
    int16_t buffer[490];
    //  Now, open the PRU character device.
    //  Read data from it in chunks and write to the named pipe.
    ssize_t readpru, writepipe, prime_char, pru_clock_command;
    pru_data = open("/dev/rpmsg_pru30", O_RDWR);
    if (pru_data < 0)
        printf("Failed to open pru character device rpmsg_pru30.");
    //  The character device must be "primed".
    prime_char = write(pru_data, "prime", 6);
    if (prime_char < 0)
        printf("Failed to prime the PRU0 char device.");
    //  Now open the PRU1 clock control char device and start the clock.
    pru_clock = open("/dev/rpmsg_pru31", O_RDWR);
    // start reading the adc
    printf("Syncing with pru_clock");
    pru_clock_command = write(pru_clock, "g", 2);
    if (pru_clock_command < 0)
        printf("The pru clock start command failed.");

    while (1) {
        readpru = read(pru_data, buffer, 62);
        if (readpru > 0) {
            for (int j = 0; j < 30; j++) {
                if ((buffer[j] >> 8) == 0) {
                    // GPIO Input value
                    bool gpio6 = (buffer[j] >> 6) & 1;
                    bool gpio7 = (buffer[j] >> 7) & 1;
                    publish("GPIO6", gpio6);
                    publish("GPIO7", gpio7);
                } else {
                    // ADC values
                    int16_t value = (buffer[j] & 0x0fff);
                    int16_t tone = getTone(value);
                    if ((buffer[j] >> 12) == 0x1) {
                        publish("ADC02", tone);
                    } else if ((buffer[j] >> 12) == 0x2) {
                        publish("ADC03", tone);
                    }
                }
            }
        }
        // tell PRU that we processed the values
        write(pru_data, "ok", 3);
    }
}

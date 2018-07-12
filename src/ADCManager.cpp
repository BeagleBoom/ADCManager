#include <fcntl.h>
#include <unistd.h>
#include <EventQueue/MessageQueue.h>
#include <EventQueue/EventType.h>

#define REFERENCE_VCC 5

bool sendChannel1 = false;
bool sendChannel2 = false;

bool canReadADCValues = false;
bool canWriteOnMessageQueue = true;
int queueListeningAddress = -1;


int getTone(int16_t value) {
    float_t oneToneSize = 0xfff / (REFERENCE_VCC * 12);
    return (int) lround(0.5 + (value / oneToneSize));
}

struct ADCOut {
    bool gpio6 = false;
    bool gpio7 = false;
    int tone_adc02 = 0;
    int tone_adc03 = 0;
    int adc0 = 0;
    int adc1 = 0;
    int adc4 = 0;
    int adc5 = 0;
};

void publish(ADCOut out, int queueValue) {
    MessageQueue queue = MessageQueue(queueValue);
    std::cout << out.adc0 << "; " << out.adc1 << "; " << out.tone_adc02 << "; " << out.tone_adc03 << "; " << out.adc4
              << "; " << out.adc5 << "; " << out.gpio6 << "; " << out.gpio7 << std::endl;
    Event event = Event(EventType::ADC_VALUES);
    event.addString("ADC");
    /*  enable if needed
        event.addInt(0);
        event.addInt(out.adc0);

        event.addInt(1);
        event.addInt(out.adc1);
        */
    event.addInt(2);
    if (sendChannel1) {
        event.addInt(out.tone_adc02);
        event.addInt(6);
        event.addInt(static_cast<int>(out.gpio6));
    } else {
        event.addInt(0);
        event.addInt(6);
        event.addInt(0);

    }
    event.addInt(3);
    if (sendChannel2) {
        event.addInt(out.tone_adc03);
        event.addInt(7);
        event.addInt(static_cast<int>(out.gpio7));
    } else {
        event.addInt(0);
        event.addInt(7);
        event.addInt(0);
    }

    /* enable if needed
        event.addInt(4);
        event.addInt(out.adc4);

        event.addInt(5);
        event.addInt(out.adc5);
    */


    queue.send(event);
}

int main(int argc, char **argv) {
    int pru_data, pru_clock; // file descriptors
    ADCOut oldData;
    //  Open a file in write mode.

    if ((argc != 2) && (argc != 3)) {
        std::cout << "usage: " << argv[0] << " <Queue Destination Address>" << std::endl;
        std::cout << "\tor: " << argv[0] << " <Queue Destination Address> <Queue Listening Address>" << std::endl;
        return -1;
    }

    int queueValue = std::stoi(argv[1]);
    MessageQueue listeningQueue = MessageQueue(queueValue);

    if (argc == 3) {
        queueListeningAddress = std::stoi(argv[2]);
        listeningQueue = MessageQueue(queueListeningAddress);
        canWriteOnMessageQueue = false;
        canReadADCValues = false;
        std::cout << "Listening on Queue-Address: " << queueListeningAddress << std::endl;
    } else {
        queueListeningAddress = -1;
        canReadADCValues = true;
        canWriteOnMessageQueue = true;
        std::cout << "Don't listening on any Queue-Address, just firing ADC Values to the Queue-Address: " << queueValue
                  << std::endl;
    }


    int16_t buffer[490];
    //  Now, open the PRU character device.
    //  Read data from it in chunks and write to the named pipe.
    ssize_t readpru, prime_char, pru_clock_command;
    pru_data = open("/dev/rpmsg_pru30", O_RDWR);
    if (pru_data < 0) {
        std::cout << "Failed to open pru character device rpmsg_pru30." << std::endl;
    }

    //  The character device must be "primed".
    prime_char = write(pru_data, "prime", 6);
    if (prime_char < 0)
        std::cout << "Failed to prime the PRU0 char device." << std::endl;
    //  Now open the PRU1 clock control char device and start the clock.
    pru_clock = open("/dev/rpmsg_pru31", O_RDWR);
    // start reading the adc
    std::cout << "Syncing with pru_clock" << std::endl;
    pru_clock_command = write(pru_clock, "g", 2);
    if (pru_clock_command < 0) {
        std::cout << "The pru clock start command failed." << std::endl;
        return 0;
    }
    std::cout << "... done!" << std::endl;

    while (true) {
        canWriteOnMessageQueue = true;

        // Getting commands from MessageQueue:
        if (queueListeningAddress != -1) {
            // Reading Events from Queue:
            Event event = listeningQueue.receiveNoWait();
            switch (event.getType()) {
                case EventType::ADC_START:
                    canReadADCValues = true;

                    /* One-Time mechanism to prevent old data from being sent on ADC_START,
                     * so the first values will be ignored from RAM cache (between PRU and CPU) */
                    canWriteOnMessageQueue = false;

                    std::cout << "Event:\t\"ADC_START\" received" << std::endl;
                    break;

                case EventType::ADC_STOP:
                    canReadADCValues = false;
                    std::cout << "Event:\t\"ADC_STOP\" received" << std::endl;
                    break;

                default:
                    break;
            }
        }

        if (canReadADCValues) {

            readpru = read(pru_data, buffer, 14);
            if (readpru == -1) break;
            if (readpru > 0) {
                ADCOut out;
                for (int j = 0; j < 7; j++) {
                    if ((buffer[j] & 0x8000)) { // first bit is "1", so it's an GPIO value
                        // GPIO Input value
                        int16_t value = buffer[j] & 0x00FF;
                        out.gpio6 = static_cast<bool>((value >> 6) & 1);
                        out.gpio7 = static_cast<bool>((value >> 7) & 1);
                        if (!sendChannel1 && out.gpio6) {
                            sendChannel1 = true;
                        }
                        if (!sendChannel2 && out.gpio7) {
                            sendChannel2 = true;
                        }
                    } else {
                        // ADC values
                        int16_t value = static_cast<int16_t>(buffer[j] & 0x0fff);
                        int tone = getTone(value);
                        int16_t adcIndex = buffer[j] >> 12;
                        switch (adcIndex) {
                            case 0x0: {
                                out.adc0 = value;
                                break;
                            }
                            case 0x1: {
                                out.adc1 = value;
                                break;
                            }
                            case 0x2: {
                                out.tone_adc02 = tone;
                                break;
                            }
                            case 0x3: {
                                out.tone_adc03 = tone;
                                break;
                            }
                            case 0x4: {
                                out.adc4 = value;
                                break;
                            }
                            case 0x5: {
                                out.adc5 = value;
                                break;
                            }
                        }
                    }
                }

                if (oldData.tone_adc02 != out.tone_adc02 ||
                    oldData.gpio6 != out.gpio6) {
                    memcpy(&oldData, &out, sizeof(ADCOut));

                    if (canWriteOnMessageQueue) {
                        publish(out, queueValue);
                    }
                }


                // tell PRU that we processed the last values
                write(pru_data, "ok", 3);
            }
        }
        usleep(1000);
    }
}
#ifndef IO_H
#define IO_H

#include "sensor_grid.h"
#include <RH_RF95.h>
/* Modules */

#if defined(__AVR_ATmega32U4__)
    #define RFM95_CS 8
    #define RFM95_RST 4
    #define RFM95_INT 7
#elif defined(ARDUINO_ARCH_SAMD)
    #define RFM95_CS 8
    #define RFM95_RST 4
    #define RFM95_INT 3
#endif

#if DUST_SENSOR == SHARP_GP2Y1010AU0F
    #include "modules/dust/SHARP_GP2Y1010AU0F.h"
#endif

extern RH_RF95 rf95;

void setupRadio();
void printMessageData();
bool postToAPI();
void transmit();
void receive();
void writeToSD(char* filename, char* str);

#endif

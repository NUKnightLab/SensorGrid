#ifndef IO_H
#define IO_H

#include <RH_RF95.h>
/* Modules */
#if WIFI_MODULE == WINC1500
    #include "modules/wifi/WINC1500.h"
#endif

#if defined(__AVR_ATmega32U4__)
    #define RFM95_CS 8
    #define RFM95_RST 4
    #define RFM95_INT 7
#elif defined(ARDUINO_ARCH_SAMD)
    #define RFM95_CS 8
    #define RFM95_RST 4
    #define RFM95_INT 3
#endif

extern RH_RF95 rf95;

void clearMessage();
void setupRadio();
void sendCurrentMessage();
void printMessageData();
bool postToAPI();

#endif

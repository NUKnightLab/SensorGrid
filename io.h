#ifndef IO_H
#define IO_H

#include <RH_RF95.h>
#include "SensorGrid.h"

#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 3

void setupRadio(uint8_t nodeID);
void printMessageData();
bool postToAPI();
void transmit();
void transmitCurrentData();
void reTransmitOldestHistory();
void receive();
void writeToSD(char* filename, char* str);
void sendBeacon();
void expireParents();

#endif

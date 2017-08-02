#ifndef IO_H
#define IO_H

#include <RHMesh.h>
#include <RHRouter.h>
#include <RH_RF95.h>
#include "SensorGrid.h"

#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 3

void setupRadio();
void printMessageData();
bool postToAPI();
bool transmit();
void reTransmitOldestHistory();
void receive();
void writeToSD(char* filename, char* str);
bool channelActive();

#endif

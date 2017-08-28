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
bool postToAPI();
bool sendCurrentMessage();
void receive();
void waitForInstructions();
void collectFromNode(int toID, uint32_t nextCollectTime);
void writeToSD(char* filename, char* str);
void fillCurrentMessageData();
void printMessageData(int fromNode);

#endif

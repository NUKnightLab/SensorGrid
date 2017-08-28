#ifndef IO_H
#define IO_H

#include <RHMesh.h>
#include <RHRouter.h>
#include <RH_RF95.h>
#include "display.h"
#include "config.h"
#include <SPI.h>
#include <SdFat.h>
#include <RHDatagram.h>

#define SD_CHIP_SELECT_PIN 10
#define RH_MESH_MAX_MESSAGE_LEN 60
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

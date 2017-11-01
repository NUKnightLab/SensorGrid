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
#include <WiFi101.h>

#define SD_CHIP_SELECT_PIN 12
#define RH_MESH_MAX_MESSAGE_LEN 60
#define RFM95_CS 10
#define RFM95_RST 11
#define RFM95_INT 6

void setupRadio();
void reconnectClient(WiFiClient& client);
void postToAPI(WiFiClient& client,int fromNode, int ID);
bool sendCurrentMessage();
void receive();
void waitForInstructions();
void collectFromNode(int toID, uint32_t nextCollectTime, WiFiClient& client);
void writeToSD(char* filename, char* str);
void fillCurrentMessageData();
void printMessageData(int fromNode);


#endif

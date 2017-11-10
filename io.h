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

#define RH_MESH_MAX_MESSAGE_LEN 60

void setupRadio(RH_RF95 rf95);
void reconnectClient(WiFiClient& client, char* ssid);
void postToAPI(WiFiClient& client,int fromNode, int ID);
bool sendCurrentMessage(RH_RF95 rf95);
void receive();
void waitForInstructions(RH_RF95 rf95);
//void collectFromNode(int toID, uint32_t nextCollectTime, WiFiClient& client, char* ssid, RH_RF95 rf95);
void collectFromNode(int toID, uint32_t nextCollectTime, WiFiClient& client, char* ssid);
void writeToSD(char* filename, char* str);
void fillCurrentMessageData();
void printMessageData(int fromNode);


#endif

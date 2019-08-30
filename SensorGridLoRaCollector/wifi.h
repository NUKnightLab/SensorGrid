#ifndef KNIGHTLAB_WIFI
#define KNIGHTLAB_WIFI

#include <WiFi101.h>
#define WIFI_CS 8
#define WIFI_IRQ 7
#define WIFI_RST 4
#define WIFI_EN 2

void printWiFiStatus();
void connectWiFi(char ssid[], char pass[], char host[], int port);
void disconnectWiFi();
byte printWiFi(char* s);
byte printlnWiFi(char* s);
byte printWiFi(int i);
byte printlnWiFi(int i);
void receiveWiFiResponse();
void receiveWiFiResponse(char buffer[], size_t len);
#endif
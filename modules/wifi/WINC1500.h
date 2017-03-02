#ifndef WINC1500_H
#define WINC1500_H

#if defined(ARDUINO_ARCH_SAMD)
#include <WiFi101.h>
extern WiFiClient WIFI_CLIENT;
#endif


void printWifiStatus();
bool postToAPI(const char* wifi_ssid, const char* wifi_pass, const char* apiServer, const char* apiHost, const int apiPort, uint8_t msgBytes[], uint8_t msgLen);
bool setupWiFi();
bool postToAPI();

#endif

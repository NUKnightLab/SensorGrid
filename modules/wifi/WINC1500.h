#ifndef WINC1500_H
#define WINC1500_H

#include <WiFi101.h>

extern WiFiClient WIFI_CLIENT;

void printWifiStatus();
void connectWiFi(const char* wifi_ssid, const char* wifi_pass);
void setupWiFi();

#endif

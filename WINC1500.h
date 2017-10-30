#ifndef WINC1500_H
#define WINC1500_H
#include <WiFi101.h>


#if defined(__AVR_ATmega32U4__)
    #include <stdint.h>
    #include <SPI.h>
    #define Serial SerialUSB
#elif defined(ARDUINO_ARCH_SAMD)
    #include <WiFi101.h>
    extern WiFiClient WIFI_CLIENT;
#else
  #error Unsupported architecture
#endif


void printWifiStatus();
// bool postToAPI(const char* wifi_ssid, const char* wifi_pass, const char* apiServer, const char* apiHost, const int apiPort, uint8_t msgBytes[], uint8_t msgLen);
bool postToAPI(const char* wifi_ssid, const char* wifi_pass, const char* apiServer, const char* apiHost, const int apiPort, char* msg, uint8_t msgLen);
bool setupWiFi(const char* wifi_ssid, const char* wifi_pass);
void connectToServer(WiFiClient& client,char ssid[],char pass[]);

#endif

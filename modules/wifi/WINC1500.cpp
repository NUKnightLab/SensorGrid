#include "WINC1500.h"

static bool WiFiPresent = false;
//static uint8_t msgLen = sizeof(Message);
//static char* buf = char[sizeof(Message)];

#if defined(ARDUINO_ARCH_SAMD)
WiFiClient WIFI_CLIENT;

void printWifiStatus() {
    Serial.print(F("SSID: "));
    Serial.print(WiFi.SSID());
    Serial.print(F("; IP: "));
    Serial.print(WiFi.localIP());
    Serial.print(F("; RSSI:"));
    Serial.print(WiFi.RSSI());
    Serial.println(F(" dBm"));
}

static bool connectWiFi(const char* wifi_ssid, const char* wifi_pass) {
    WiFi.end(); // This is here to make the keep-alive work, but not sure
                // why we are even having to reconnect for every request
    Serial.println("Connecting ...");
    if (WiFi.status() != WL_CONNECTED) {
        Serial.print(F("CON SSID: "));
        Serial.println(wifi_ssid);
        // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
        //WIFI_STATUS = WiFi.begin(WIFI_SSID, WIFI_PASS);
        WiFi.begin(wifi_ssid, wifi_pass);
        uint8_t timeout = 10;
        while (timeout && (WiFi.status() != WL_CONNECTED)) {
            timeout--;
            delay(1000);
        }
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WARNING: Could not connect to WiFi");
        return false;
	}
    Serial.println(F("WIFI:\n   "));
    printWifiStatus();
	return true;
}

//bool postToAPI(const char* wifi_ssid, const char* wifi_pass, const char* apiServer, const char* apiHost, const int apiPort, uint8_t msgBytes[], uint8_t msgLen) {
bool postToAPI(const char* wifi_ssid, const char* wifi_pass, const char* apiServer, const char* apiHost, const int apiPort, char* msg, uint8_t msgLen) {
	if (!WiFiPresent || !connectWiFi(wifi_ssid, wifi_pass)) {
	   return false;
	}
    if (WIFI_CLIENT.connect(apiServer, apiPort)) {
        Serial.println(F("API:"));
        Serial.print(F("    CON: "));
        Serial.print(apiServer);
        Serial.print(F(":")); Serial.println(apiPort);
        //char* messageChars = (char*)msgBytes;
        //char* messageChars = (char*)*msg;
        WIFI_CLIENT.println("POST /data HTTP/1.1");
        WIFI_CLIENT.print("Host: "); WIFI_CLIENT.println(apiHost);
        WIFI_CLIENT.println("User-Agent: ArduinoWiFi/1.1");
        WIFI_CLIENT.println("Connection: close");
        WIFI_CLIENT.println("Content-Type: application/x-www-form-urlencoded");
        WIFI_CLIENT.print("Content-Length: "); WIFI_CLIENT.println(msgLen);
        Serial.print(F("Message length is: ")); Serial.println(msgLen);
        WIFI_CLIENT.println();
        WIFI_CLIENT.write(msg, msgLen);
        WIFI_CLIENT.println();
        return true;
    } else {
      Serial.println(F("FAIL: API CON"));
    }
    return false;
}
#else
//bool postToAPI(const char* wifi_ssid, const char* wifi_pass, const char* apiServer, const char* apiHost, const int apiPort, uint8_t msgBytes[], uint8_t msgLen) {
bool postToAPI(const char* wifi_ssid, const char* wifi_pass, const char* apiServer, const char* apiHost, const int apiPort, char* msg, uint8_t msgLen) {
    return false;
}

#endif

bool setupWiFi() {
    Serial.print(F("WiFi Module.. "));
    #if defined(__AVR_ATmega32U4__)
        Serial.println(F(" ..Not Supported on 32u4 devices"));
    #elif defined(ARDUINO_ARCH_SAMD)
        WiFi.setPins(10,11,12); // Adafruit uses 8,7,4 in tutorials, but these are used by LoRa
    if (WiFi.status() == WL_NO_SHIELD) {
        Serial.print(F(" ..Not detected"));
    } else {
        WiFiPresent = true;
        Serial.println(F(" ..Detected"));
    }
    #else
        #error Unsupported architecture
    #endif
    return WiFiPresent;
}

#include "WINC1500.h"

static bool WiFiPresent = false;

WiFiClient WIFI_CLIENT;

void printWifiStatus()
{
    Serial.print(F("SSID: "));
    Serial.print(WiFi.SSID());
    Serial.print(F("; IP: "));
    Serial.print(WiFi.localIP());
    Serial.print(F("; RSSI:"));
    Serial.print(WiFi.RSSI());
    Serial.println(F(" dBm"));
}

static bool connectWiFi(const char* wifi_ssid, const char* wifi_pass)
{
    //WiFi.end(); // This is here to make the keep-alive work, but not sure
                  // why we are even having to reconnect for every request
    Serial.println("Checking connection ...");
    if (WiFi.status() != WL_CONNECTED) {
        Serial.print("WiFi Status: "); Serial.println(WiFi.status());
        Serial.print(F("CON SSID: "));
        Serial.println(wifi_ssid);
        Serial.print(F("CON PASS: ")); Serial.println(wifi_pass);
        // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
        //WIFI_STATUS = WiFi.begin(WIFI_SSID, WIFI_PASS);
        Serial.println("Connecting ..");
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

extern "C" char *sbrk(int i);
static int _freeRam()
{
  char stack_dummy = 0;
  return &stack_dummy - sbrk(0);
}

static bool _postToAPI(const char* apiServer, const char* apiHost, const int apiPort, char* msg, uint8_t msgLen)
{
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
        Serial.println(F("Completed post to API"));
        Serial.println(_freeRam());
        return true;
    } else {
        return false;
    }
}

//bool postToAPI(const char* wifi_ssid, const char* wifi_pass, const char* apiServer, const char* apiHost, const int apiPort, uint8_t msgBytes[], uint8_t msgLen) {
bool postToAPI(const char* wifi_ssid, const char* wifi_pass, const char* apiServer, const char* apiHost, const int apiPort, char* msg, uint8_t msgLen)
{
	if (!WiFiPresent || !connectWiFi(wifi_ssid, wifi_pass)) {
	   return false;
	}
    if (_postToAPI(apiServer, apiHost, apiPort, msg, msgLen)) {
        return true;
    } else {
      Serial.println(F("FAIL: API CON. RETRY .."));
      WiFi.end();
      if (connectWiFi(wifi_ssid, wifi_pass)) {
          _postToAPI(apiServer, apiHost, apiPort, msg, msgLen);
      }
    }
    return false;
}

bool setupWiFi(const char* wifi_ssid, const char* wifi_pass)
{
    Serial.print(F("WiFi Module.. "));
    #if defined(ARDUINO_ARCH_SAMD)
        /**
         * void setPins(int8_t cs, int8_t irq, int8_t rst, int8_t en = -1)
         *
         * cs: pull down during rx/tx
         *
         */
        WiFi.setPins(6,11,12); // Adafruit uses 8,7,4 in tutorials, but these are used by LoRa
                                // Adalogger uses 10
    if (WiFi.status() == WL_NO_SHIELD) {
        Serial.println(F(" ..Not detected"));
    } else {
        WiFiPresent = true;
        Serial.println(F(" ..Detected"));
        Serial.println(F("Connecting to WiFi.."));
        connectWiFi(wifi_ssid, wifi_pass);
    }
    #else
        #error Unsupported architecture
    #endif
    return WiFiPresent;
}

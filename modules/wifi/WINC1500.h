#include <WiFi101.h>

int WIFI_STATUS = WL_IDLE_STATUS;
WiFiClient WIFI_CLIENT;

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print(F("SSID: "));
  Serial.print(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print(F("; IP: "));
  Serial.print(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print(F("; RSSI:"));
  Serial.print(rssi);
  Serial.println(F(" dBm"));
}


void setupWiFi() {
  WiFi.setPins(10,11,12); // Adafruit uses 8,7,4 in tutorials, but these are used by LoRa

  // Check for the presence of the shield
  if (WiFi.status() == WL_NO_SHIELD) {
    fail(WIFI_MODULE_NOT_DETECTED);
  }
  Serial.println(F("WiFi Module Detected"));

  while (WIFI_STATUS != WL_CONNECTED) {
    Serial.print(F("CON SSID: "));
    Serial.println(WIFI_SSID);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    WIFI_STATUS = WiFi.begin(WIFI_SSID, WIFI_PASS);
    // wait 10 seconds for connection:
    delay(10000);
  }
  Serial.println(F("WIFI: "));
  printWifiStatus();

  Serial.println(F("\nCON SERVER..."));
  if (WIFI_CLIENT.connect(API_SERVER, API_PORT)) {
    Serial.println(F("!CON"));
    WIFI_CLIENT.println(F("GET /?status=START HTTP/1.1"));
    WIFI_CLIENT.print(F("Host: ")); WIFI_CLIENT.println(API_HOST);
    WIFI_CLIENT.println(F("Connection: close"));
    WIFI_CLIENT.println();
  }
}

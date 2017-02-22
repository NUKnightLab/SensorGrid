#include <WiFi101.h>

//int WIFI_STATUS = WL_IDLE_STATUS;
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


void connectWiFi() {
  Serial.print("Checking WiFi connection ... ");
  //while (WIFI_STATUS != WL_CONNECTED) {
  /* This is an unreliable check
  if (WiFi.RSSI() == 0) {
      Serial.print("No WiFi signal. Reconnecting ... ");
      WiFi.end();
  } else {
      Serial.print("RSSI: "); Serial.println(WiFi.RSSI());
  }
  */

  /*
  if (WiFi.status() == WL_CONNECTED) {
      Serial.print("Already connected (RSSI: ");
      Serial.print(WiFi.RSSI()); Serial.println(")");
      return;
  }
  */
  WiFi.end(); // This is here to make the keep-alive work, but not sure
              // why we are even having to reconnect for every request
  Serial.println("Connecting ...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(F("CON SSID: "));
    Serial.println(WIFI_SSID);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    //WIFI_STATUS = WiFi.begin(WIFI_SSID, WIFI_PASS);
    // wait 10 seconds for connection:
    //delay(10000);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    // wait up to 10 seconds for connection:
    uint8_t timeout = 10;
    while (timeout && (WiFi.status() != WL_CONNECTED)) {
        timeout--;
        delay(1000);
    }
  }
  Serial.println(F("WIFI: "));
  printWifiStatus();
}

void setupWiFi() {
  WiFi.setPins(10,11,12); // Adafruit uses 8,7,4 in tutorials, but these are used by LoRa

  // Check for the presence of the shield
  if (WiFi.status() == WL_NO_SHIELD) {
    fail(WIFI_MODULE_NOT_DETECTED);
  }
  Serial.println(F("WiFi Module Detected"));

  /*
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
  */
  connectWiFi();

  /*
  Serial.println(F("\nCON SERVER..."));
  if (WIFI_CLIENT.connect(API_SERVER, API_PORT)) {
    Serial.println(F("!CON"));
    //WIFI_CLIENT.println("GET /?status=START HTTP/1.1");
    WIFI_CLIENT.println("POST /data HTTP/1.1");
    WIFI_CLIENT.print("Host: "); WIFI_CLIENT.println(API_HOST);
    WIFI_CLIENT.println("Connection: close");
    WIFI_CLIENT.println("Content-Type: application/x-www-form-urlencoded");
    WIFI_CLIENT.print("Content-Length: "); WIFI_CLIENT.println(msgLen);
    WIFI_CLIENT.println();
    WIFI_CLIENT.write(msgBytes, msgLen);
    WIFI_CLIENT.println();
  } else {
      Serial.println("FAILED WIFI TEST");
  }
  */
}

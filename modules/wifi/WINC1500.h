int WIFI_STATUS = WL_IDLE_STATUS;
WiFiClient WIFI_CLIENT;

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}


void setupWiFi() {
  WiFi.setPins(10,11,12); // Adafruit uses 8,7,4 in tutorials, but these are used by LoRa

  // Check for the presence of the shield
  Serial.print("WiFi101 shield: ");
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi Module Not Present");
    return; // don't continue
  }
  Serial.println("WiFi Module Detected");

  while (WIFI_STATUS != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(WIFI_SSID);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    WIFI_STATUS = WiFi.begin(WIFI_SSID, WIFI_PASS);
    // wait 10 seconds for connection:
    delay(10000);
  }
  Serial.println("Connected to wifi");
  printWifiStatus();

  Serial.println("\nStarting connection to server...");
  // if you get a connection, report back via serial:
  if (WIFI_CLIENT.connect(API_SERVER, API_PORT)) {
    Serial.println("connected to server");
    // Make a HTTP request:
    WIFI_CLIENT.println("GET /?status=START HTTP/1.1");
    WIFI_CLIENT.print("Host: "); WIFI_CLIENT.println(API_HOST);
    WIFI_CLIENT.println("Connection: close");
    WIFI_CLIENT.println();
  }
}

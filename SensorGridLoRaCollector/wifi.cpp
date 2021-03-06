#include <console.h>
#include "wifi.h"

static WiFiClient client;
//WiFiSSLClient client;

static bool wifi_present = true;

WiFiClient wifiClient()
{
    return client;
}

void printWiFiStatus()
{
    println("SSID: %s; IP: %s; RSSI: %l", WiFi.SSID(), WiFi.localIP(), WiFi.RSSI());
}

void connectWiFi(char ssid[], char pass[], char host[], int port)
{
    println("Setting pins CS: %d; IRQ: %d; RST: %d; EN: %d",
        WIFI_CS, WIFI_IRQ, WIFI_RST, WIFI_EN);
    WiFi.setPins(WIFI_CS, WIFI_IRQ, WIFI_RST, WIFI_EN);
    int status = WL_IDLE_STATUS;
    while (status!= WL_CONNECTED) {
        Serial.print("Attempting to connect to SSID: ");
        Serial.println(ssid);
        Serial.print("Using password: ");
        Serial.println(pass);
        status = WiFi.begin(ssid, pass);
        Serial.println("Connected to WiFi");
        Serial.print("\nStarting connection ");
        Serial.print(host); Serial.print(":"); Serial.println(port);
        if (client.connect(host, port)) {
            Serial.println("connected to server");
        } else {
            Serial.println("server connection failed");
        }
    }
    Serial.println(".. connected");
}

void disconnectWiFi()
{
    client.stop();
    WiFi.end();
}

/*
void reconnectClient(WiFiClient &client, char* ssid)
{
    client.stop();
    Serial.print("Reconnecting to ");
    Serial.print(config.api_host);
    Serial.print(":");
    Serial.println(config.api_port);
    if (client.connect(config.api_host, config.api_port)) {
        Serial.println("connecting ...");
    } else {
        Serial.println("Failed to reconnect");
    }
}
*/

byte printWiFi(char* s)
{
    return client.print(s);
}

byte printlnWiFi(char* s)
{
    return client.println(s);
}

byte printWiFi(int i)
{
    return client.print(i);
}

byte printlnWiFi(int i)
{
    return client.println(i);
}

void receiveWiFiResponse()
{
    Serial.println("Receiving server response ..");
    while (!client.available()) {}
    while (client.available()) {
        char c = client.read();
        Serial.write(c);
    }
    println("********** done");
}

void readHeader()
{
    bool blank_line = true;
    while(client.available()) {
        char c = client.read();
        Serial.print(c);
        if (c == '\n') {
            if (blank_line) {
                Serial.println("--");
                Serial.println("Done reading header.");
                break;
            }
            blank_line = true;
        } else if (c != '\r') {
            blank_line = false;
        }
    }
}

void receiveWiFiResponse(char buffer[], size_t len)
{
    // TODO: see https://arduinojson.org/v6/example/http-client/
    Serial.println("Receiving buffered server response ..");
    while (!client.available()) {}
    readHeader();
    int i = 0;
    while (i < len - 1 && client.available()) {
        char c = client.read();
        buffer[i++] = c;
        Serial.write(c);
    }
    buffer[i] = '\0';
    Serial.print("i: ");
    Serial.println(i);
    Serial.print("buffer: ");
    Serial.println(buffer);
    println("********** done");
}

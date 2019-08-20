#include <LoRa.h>
#include "config.h"
#include <LoRaHarvest.h>
#include <console.h>
#include <DataManager.h>
#include "wifi.h"


extern "C" char *sbrk(int i);

int FreeRam () {
  char stack_dummy = 0;
  return &stack_dummy - sbrk(0);
}

static unsigned long previousMillis = 0;
static unsigned long interval = 60000;
static uint8_t seq = 0;

bool runEvery()
{
    uint32_t currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        return true;
    }
    return false;
}

uint32_t nextCollection()
{
    return previousMillis + interval;
}

bool scheduleDataSample(unsigned long interval)
{
    static unsigned long previousMillis = 0;
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        return true;
    }
    return false;
}

void sendDataToApi(uint8_t node_id)
{
    println("***** SEND DATA TO API *****");
    int msg_length = 0;
    for (int i=0; i<numBatches(0); i++) {
        char *batch = (char*)getBatch(i);
        msg_length += strlen(batch);
        if (i < numBatches(0)-1) {
            msg_length++;
        }
    }
    msg_length += 13;
    println("Message length: %d", msg_length);
    println("Free ram: %d", FreeRam());
    digitalWrite(WIFI_CS, LOW);
    if (true) {
        connectWiFi(config.wifi_ssid, config.wifi_password, config.api_host, config.api_port);
        printWiFi("POST /networks/");
        printWiFi(config.network_id);
        printWiFi("/nodes/");
        printWiFi(node_id);
        printlnWiFi("/data/ HTTP/1.1");
        printWiFi("Host: ");
        printlnWiFi(config.api_host);
        printlnWiFi("Content-Type: application/json");
        printWiFi("Content-Length: ");
        printlnWiFi(msg_length);
        printlnWiFi("");
        Serial.println(".. posted header");
    }
    printWiFi("{ \"data\": [");
    for (int i=0; i<numBatches(0); i++) {
        char *batch = (char*)getBatch(i);
        print(batch);
        printWiFi(batch);
        if (i < numBatches(0)-1) {
            printWiFi(",");
        }
    }
    printlnWiFi("]}");
    receiveWiFiResponse();
    disconnectWiFi();
    digitalWrite(WIFI_CS, HIGH);
    SPI.endTransaction();
    clearData();
}

void setup()
{
    unsigned long _start = millis();
    while ( !Serial && (millis() - _start) < 10000 ) {}
    loadConfig();
    nodeId(config.node_id);
    println("Config loaded");
    println("Configuring LoRa with pins: CS: %d; RST: %d; IRQ: %d",
        config.RFM95_CS, config.RFM95_RST, config.RFM95_INT);
    LoRa.setPins(config.RFM95_CS, config.RFM95_RST, config.RFM95_INT);
    if (!LoRa.begin(915E6)) {
        Serial.println("LoRa init failed");
        while(true);
    }
    LoRa.enableCrc();
    LoRa.onReceive(collector_onReceive);
    rtcz.begin();
    connectWiFi(config.wifi_ssid, config.wifi_password, config.api_host, config.api_port);
    disconnectWiFi();
    LoRa.receive();
}

void tick()
{
    static unsigned long tick_time = 0;
    if (millis() > tick_time) {
        print(".");
        tick_time = millis() + 1000;
    }
}

void loop() {
    static uint8_t nodes_collected[255] = {0};
    tick();
    if (readyToPost() > 0) {
        LoRa.idle();
        digitalWrite(config.RFM95_CS, HIGH);
        SPI.endTransaction();
        sendDataToApi(readyToPost());
        nodes_collected[readyToPost()] = 1;
        readyToPost(0);
        digitalWrite(config.RFM95_CS, LOW);
        /* A fresh begin seems to be required for LoRa to recover the SPI bus from WiFi */
        if (!LoRa.begin(915E6)) {
            Serial.println("LoRa init failed");
            while(true);
        }
        LoRa.receive();
        delay(1000);
        for (int i=0; i<sizeof(nodes); i++) {
            if (nodes_collected[nodes[i]] == 0) {
                sendCollectPacket(nodes[i], 0, ++++seq);
                return;
            }
        }
        memset(nodes_collected, 0, sizeof(nodes_collected));
        sendStandby(++++seq, nextCollection());
    }
    if (runEvery()) sendCollectPacket(2, 0, ++++seq);
}
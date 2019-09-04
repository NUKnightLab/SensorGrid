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

void fetchRoutes()
{
    println("***** FETCH ROUTES FROM API *****");
    //int msg_length = 0;
    //msg_length += 13;
    //println("Message length: %d", msg_length);
    //println("Free ram: %d", FreeRam());
    size_t n = 255;
    char json[n];
    digitalWrite(WIFI_CS, LOW);
    if (true) {
        connectWiFi(config.wifi_ssid, config.wifi_password, config.api_host, config.api_port);
        printWiFi("GET /networks/");
        printWiFi(config.network_id);
        printlnWiFi("/nodes HTTP/1.1");
        printWiFi("Host: ");
        printlnWiFi(config.api_host);
        printlnWiFi("Content-Type: application/json");
        printlnWiFi("Connection: close");
        printlnWiFi("");
        Serial.println(".. posted header");
    }
    //printWiFi("{ \"data\": [");
    //for (int i=0; i<numBatches(0); i++) {
    //    char *batch = (char*)getBatch(i);
    //    print(batch);
    //    printWiFi(batch);
    //    if (i < numBatches(0)-1) {
    //        printWiFi(",");
    //    }
    //}
    //printlnWiFi("]}");
    receiveWiFiResponse(json, n);
    disconnectWiFi();
    digitalWrite(WIFI_CS, HIGH);
    SPI.endTransaction();
    Serial.println("ROUTES:");
    Serial.println(json);
    parseRoutingTable(json);
    Serial.println("----");
    Serial.print("Node count: "); Serial.println(node_count);
    for (int i=0; i<node_count; i++) {
        Serial.print(nodes[i]); Serial.print(" ");
    }
    Serial.println("----");
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
    isCollector(true);
    println("Config loaded. Fetching routes ..");
    fetchRoutes();

    /* set time */
    connectWiFi(config.wifi_ssid, config.wifi_password, config.api_host, config.api_port);
    uint32_t wifi_time = 0;
    Serial.println("Getting time from NTP server ");
    while (wifi_time == 0) {
        wifi_time = WiFi.getTime();
        Serial.print(".");
        delay(2000);
    }
    Serial.print("Setting time from Wifi module: ");
    Serial.println(wifi_time);
    rtcz.begin();
    rtcz.setEpoch(wifi_time);
    disconnectWiFi();

    println("Configuring LoRa with pins: CS: %d; RST: %d; IRQ: %d",
        config.RFM95_CS, config.RFM95_RST, config.RFM95_INT);
    LoRa.setPins(config.RFM95_CS, config.RFM95_RST, config.RFM95_INT);
    if (!LoRa.begin(915E6)) {
        Serial.println("LoRa init failed");
        while(true);
    }
    LoRa.enableCrc();
    LoRa.onReceive(onReceive);
    //LoRa.receive();
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
        Serial.print("Finding next uncollected node of node_count: ");
        Serial.println(node_count);
        for (int i=0; i<node_count; i++) {
            Serial.print("Checking if collected: ");
            Serial.println(nodes[i]);
            if (nodes_collected[nodes[i]] == 0) {
                Serial.println(".. not collected");
                sendCollectPacket(nodes[i], 0, ++++seq);
                lastRequestNode(nodes[i]);
                resetRequestTimer();
                return;
            }
        }
        //sendStandby(++++seq, nextCollection());
    }

    // TODO: should we keep a cycle timer that runs separately from the request timeout?
    if (requestTimer() > 30000) {
        Serial.println("****** TIMEOUT ******");
        // TODO: a partially collected node should be written to the API and collection continued
        // on next cycle. Will require sensors to clear collected data based on request packet id
        if (nodes_collected[lastRequestNode()] == 0) {
            nodes_collected[lastRequestNode()] = 2; // simple timeout code for now
            /*
            if (txPower(lastRequestNode()) < MAX_LORA_TX_POWER) {
                txPower(lastRequestNode(), txPower(lastRequestNode())+1);
            }
            Serial.print("Increased TX power level to unreachable node ");
            Serial.print(lastRequestNode());
            Serial.print(" to: ");
            Serial.print(txPower(lastRequestNode()));
            Serial.println(" dB");
            */
        } else if (nodes_collected[lastRequestNode()] == 2) {
            nodes_collected[lastRequestNode()] = 3;
            /*
            if (txPower(lastRequestNode()) < MAX_LORA_TX_POWER) {
                txPower(lastRequestNode(), txPower(lastRequestNode())+1);
            }
            Serial.print("Increased TX power level to unreachable node ");
            Serial.print(lastRequestNode());
            Serial.print(" to: ");
            Serial.print(txPower(lastRequestNode()));
            Serial.println(" dB");
            */
        //} else if (nodes_collected[lastRequestNode()] == 3) {
        //    nodes_collected[lastRequestNode()] = 4;
        }
        for (int i=0; i<node_count; i++) {
            Serial.print("Checking if collected: ");
            Serial.println(nodes[i]);
            if (nodes_collected[nodes[i]] == 0) {
                Serial.println(".. not collected");
                sendCollectPacket(nodes[i], 0, ++++seq);
                lastRequestNode(nodes[i]);
                resetRequestTimer();
                return;
            }
        }
        // No remaining uncollected, but check for previous timeouts
        for (int i=0; i<node_count; i++) {
            Serial.print("Checking if collected: ");
            Serial.println(nodes[i]);
            if (nodes_collected[nodes[i]] == 2) {
                Serial.println(".. not collected (previously timed out)");
                sendCollectPacket(nodes[i], 0, ++++seq);
                lastRequestNode(nodes[i]);
                resetRequestTimer();
                return;
            }
        }
        for (int i=0; i<node_count; i++) {
            if (nodes_collected[nodes[i]] == 3) {
                Serial.print("****** WARNING ****** unreachable node: ");
                Serial.println(nodes[i]);
            }
        }
        // mark all collected and reset timer
        Serial.println("Resetting collection state for next cycle");
        memset(nodes_collected, 0, sizeof(nodes_collected));
        resetRequestTimer();
    }

/*
    if (runEvery()) {
        memset(nodes_collected, 0, sizeof(nodes_collected));
        sendCollectPacket(nodes[0], 0, ++++seq);
        lastRequestNode(nodes[0]);
        resetRequestTimer();
    }
    */
}
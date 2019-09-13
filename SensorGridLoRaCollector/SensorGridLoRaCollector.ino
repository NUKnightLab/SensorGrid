#include <LoRa.h>
#include "config.h"
#include <LoRaHarvest.h>
#include <console.h>
#include <DataManager.h>
#include "wifi.h"

#define SERIAL_TIMEOUT 10000
#define COLLECTION_CODE_UNCOLLECTED 0
#define COLLECTION_CODE_COLLECTED 1
#define COLLECTION_CODE_TIMEOUT 2
#define COLLECTION_CODE_UNREACHABLE 3

static uint8_t nodes_collected[255] = {0};
extern "C" char *sbrk(int i);

int FreeRam () {
  char stack_dummy = 0;
  return &stack_dummy - sbrk(0);
}

static uint8_t seq = 0;

void fetchRoutes()
{
    println("***** FETCH ROUTES FROM API *****");
    size_t n = 255;
    char json[n];
    digitalWrite(WIFI_CS, LOW);
    connectWiFi(config.wifi_ssid, config.wifi_password, config.api_host, config.api_port);
    printWiFi("GET /networks/");
    printWiFi(config.network_id);
    printlnWiFi("/nodes HTTP/1.1");
    printWiFi("Host: ");
    printlnWiFi(config.api_host);
    printlnWiFi("Content-Type: application/json");
    printlnWiFi("Connection: close");
    printlnWiFi("");
    receiveWiFiResponse(json, n);
    disconnectWiFi();
    digitalWrite(WIFI_CS, HIGH);
    SPI.endTransaction();
    parseRoutingTable(json);
}

void _sendDataToApi(uint8_t node_id)
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

void sendDataToApi(uint8_t node_id)
{
    println("***** SENDING CHUNKED DATA TO API *****");
    digitalWrite(WIFI_CS, LOW);
    connectWiFi(config.wifi_ssid, config.wifi_password, config.api_host, config.api_port);
    printWiFi("POST /networks/"); Serial.print("POST /networks/");
    printWiFi(config.network_id); Serial.print(config.network_id);
    printWiFi("/nodes/"); Serial.print("/nodes/");
    printWiFi(node_id); Serial.print(node_id);
    printlnWiFi("/data/ HTTP/1.1"); Serial.println("/data/ HTTP/1.1");
    printWiFi("Host: "); Serial.print("Host: ");
    printlnWiFi(config.api_host); Serial.println(config.api_host);
    printlnWiFi("Content-Type: application/json"); Serial.println("Content-Type: application/json");
    printlnWiFi("Transfer-Encoding: chunked"); Serial.println("Transfer-Encoding: chunked");
    printlnWiFi(""); Serial.println("");
    printlnWiFi(11, HEX); Serial.println(11, HEX);
    printlnWiFi("{ \"data\": ["); Serial.println("{ \"data\": [");
    while (getRecordCount() > 0) {
        DataRecord record = getNextRecord();
        if (getRecordCount() > 0) {
            printlnWiFi(record.len + 1, HEX); Serial.println(record.len + 1, HEX);
            printWiFi(record.data); Serial.print(record.data);
            printlnWiFi(","); Serial.println(",");
        } else {
            printlnWiFi(record.len + 2, HEX); Serial.println(record.len + 2, HEX);
            printWiFi(record.data); Serial.print(record.data);
            printlnWiFi("]}"); Serial.println("]}");
        }
    }
    printlnWiFi(0); Serial.println(0);
    printlnWiFi(""); Serial.println("");
    println("***** DONE *****");
    receiveWiFiResponse();
    disconnectWiFi();
    digitalWrite(WIFI_CS, HIGH);
    SPI.endTransaction();
}

void setTime()
{
    const uint32_t UNSET_RTCZ_TIME = 943920000;
    rtcz.begin();
    if (rtcz.getEpoch() == UNSET_RTCZ_TIME) {
        connectWiFi(config.wifi_ssid, config.wifi_password, config.api_host, config.api_port);
        uint32_t wifi_time = 0;
        println("Getting time from NTP server ");
        while (wifi_time == 0) {
            wifi_time = WiFi.getTime();
            Serial.print(".");
            delay(2000);
        }
        println("Setting time from Wifi module: %lu", wifi_time);
        rtcz.setEpoch(wifi_time);
        disconnectWiFi();
    } else {
        println("Using preset time: %lu", rtcz.getEpoch());
    }
}

void waitSerial()
{
    unsigned long _start = millis();
    while ( !Serial && (millis() - _start) < SERIAL_TIMEOUT ) {}
}

void tick()
{
    static unsigned long tick_time = 0;
    if (millis() > tick_time) {
        print(".");
        tick_time = millis() + 1000;
    }
}

bool sendNextCollectPacket(int collection_code)
{
    for (int i=0; i<node_count; i++) {
        println("Checking if collected: %d", nodes[i]);
        if (nodes_collected[nodes[i]] == collection_code) {
            print(".. not collected");
            if (collection_code == COLLECTION_CODE_TIMEOUT) {
                println(" (previously timed out)");
            } else {
                println("");
            }
            //sendCollectPacket(nodes[i], 0, ++++seq);
            sendCollectPacket(nodes[i], 1, ++++seq);
            lastRequestNode(nodes[i]);
            resetRequestTimer();
            return true;
        }
    }
    return false; // no collection requests were sent
}

void setup()
{
    waitSerial();
    isCollector(true);
    loadConfig();
    nodeId(config.node_id);
    setTime(); 
    println("Config loaded. Fetching routes ..");
    fetchRoutes();
    println("Configuring LoRa with pins: CS: %d; RST: %d; IRQ: %d",
        config.RFM95_CS, config.RFM95_RST, config.RFM95_INT);
    LoRa.setPins(config.RFM95_CS, config.RFM95_RST, config.RFM95_INT);
    if (!LoRa.begin(915E6)) {
        println("LoRa init failed");
        while(true);
    }
    LoRa.enableCrc();
    LoRa.onReceive(onReceive);
}

void loop() {
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
        //delay(1000);
        println("Finding next uncollected node of node_count: %d", node_count);
        if (sendNextCollectPacket(COLLECTION_CODE_UNCOLLECTED)) return;
        //sendStandby(++++seq, nextCollection());
    }
    if (requestTimer() > 30000) {
        Serial.println("***** TIMEOUT *****");
        // TODO: a partially collected node should be written to the API and collection continued
        // on next cycle. Will require sensors to clear collected data based on request packet id
        if (nodes_collected[lastRequestNode()] == COLLECTION_CODE_UNCOLLECTED) {
            nodes_collected[lastRequestNode()] = COLLECTION_CODE_TIMEOUT; // simple timeout code for now
        } else if (nodes_collected[lastRequestNode()] == COLLECTION_CODE_TIMEOUT) {
            nodes_collected[lastRequestNode()] = COLLECTION_CODE_UNREACHABLE;
        }
        if (sendNextCollectPacket(COLLECTION_CODE_UNCOLLECTED)) return;
        if (sendNextCollectPacket(COLLECTION_CODE_TIMEOUT)) return;
        for (int i=0; i<node_count; i++) {
            if (nodes_collected[nodes[i]] == COLLECTION_CODE_UNREACHABLE) {
                println("***** WARNING ***** unreachable node: %d", nodes[i]);
            }
        }
        println("Resetting collection state for next cycle");
        memset(nodes_collected, 0, sizeof(nodes_collected));
        resetRequestTimer();
    }
}
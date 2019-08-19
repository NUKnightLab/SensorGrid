#include <LoRa.h>
#include "config.h"
#include <LoRaHarvest.h>
#include <console.h>
#include <DataManager.h>
#include "wifi.h"


extern "C" char *sbrk(int i);
uint8_t ready_to_post = 0;

int FreeRam () {
  char stack_dummy = 0;
  return &stack_dummy - sbrk(0);
}

void collector_writeTimestamp()
{
    uint32_t ts = rtcz.getEpoch();
    print("Sending TIMESTAMP: %u", ts);
    LoRa.write(ts >> 24);
    LoRa.write(ts >> 16);
    LoRa.write(ts >> 8);
    LoRa.write(ts);
}

uint8_t nodes[2] = { 2, 3 };
uint8_t routes[255][6] = {
    { 0 },
    { 0 },
    { 1, 2 },
    { 1, 2, 3 },
    { 1, 2, 3, 6, 5, 4 },
    { 1, 3, 6, 5 },
    { 1, 2, 3, 6 }
};

static unsigned long previousMillis = 0;
static unsigned long interval = 60000;
static uint8_t seq = 0;

bool runEvery()
{
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        return true;
    }
    return false;
}

unsigned long nextCollection()
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


void sendCollectPacket(uint8_t node_id, uint8_t packet_id)
{
    //println("prefetch collection state: collecting: %d, waiting: %d, node idx: %d, packet: %d",
    //    collectingData(), waitingPacket(), collectingNodeIndex(), packet_id);
    //waitingPacket(true);
    //uint8_t node_id = nodes[collectingNodeIndex()];
    uint8_t *route = routes[node_id];
    uint8_t route_size = 0;
    while (route_size < sizeof(route) && route[route_size] > 0) route_size++;
    Serial.print("Fetching data from: ");
    Serial.print(node_id);
    Serial.print("; ROUTE: ");
    for (int j=0; j<route_size; j++) {
        Serial.print(route[j]);
        Serial.print(" ");
    }
    Serial.println("");
    LoRa.flush();
    LoRa.idle();
    LoRa.beginPacket();
    LoRa.write(route[1]);
    LoRa.write(config.node_id);
    LoRa.write(node_id);
    LoRa.write(++++seq);
    //LoRa.write(PACKET_TYPE_SENDDATA);
    LoRa.write(1);
    collector_writeTimestamp();
    LoRa.write(route, route_size);
    LoRa.write(0); // end route
    LoRa.write(packet_id); // packet id
    LoRa.endPacket();
    //timeout = millis() + 10000;
    //println("set timeout to: %d", timeout);
    LoRa.receive();
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

void sendStandby()
{
    for (int i=0; i<sizeof(nodes); i++) {
        LoRa.flush();
        LoRa.idle();
        LoRa.beginPacket();
        LoRa.write(routes[nodes[i]][1]);
        LoRa.write(config.node_id);
        LoRa.write(nodes[i]);
        LoRa.write(++++seq);
        LoRa.write(PACKET_TYPE_STANDBY);
        //LoRa.write(3);
        collector_writeTimestamp();
        LoRa.write(routes[nodes[i]], sizeof(routes[nodes[i]]));
        LoRa.write(0); // end route
        LoRa.write(nextCollection() / 1000);
        LoRa.endPacket();
        LoRa.receive();
    }
    //collectingNodeIndex(-1);
    //collectingPacketId(1);
    //collectingData(false);
    //waitingPacket(false);
}

void collector_handleDataMessage(uint8_t from_node, uint8_t *message, size_t msg_size)
{
    uint8_t packet_id = message[0];
    static uint8_t nodes_collected[255] = {0};
    print("NODE: %d; PACKET: %d; MESSAGE:", from_node, packet_id);
    for (uint8_t i=1; i<msg_size; i++) {
        //print(" %f", (float)message[i] / 10.0);
        print("%c", message[i]);
    }
    recordData((char*)(&message[1]), msg_size-1);
    println("");
    char *batch = getCurrentBatch();
    if (packet_id == 1) { // TODO: be sure we received all available packets
        ready_to_post = from_node;
    } else {
        sendCollectPacket(from_node, --packet_id);
    }
}

int collector_handlePacket(int to, int from, int dest, int seq, int packetType, uint32_t timestamp, uint8_t *route, size_t route_size, uint8_t *message, size_t msg_size, int topology)
{
    static int last_seq = 0;
    if (seq == last_seq) {
        return 1;
        //return MESSAGE_CODE_DUPLICATE_SEQUENCE;
    }
    if (to != config.node_id && to != 255) return 2;
    //if (to != config.node_id && to != 255) return MESSAGE_CODE_WRONG_ADDRESS;
    //if (to != nodeId() && to != 255) return MESSAGE_CODE_WRONG_ADDRESS;
    //if (!topologyTest(topology, to, from)) return MESSAGE_CODE_TOPOLOGY_FAIL;
    last_seq = seq;
    uint8_t packet_id;
    if (dest == config.node_id) {
        switch (packetType) {
            //case PACKET_TYPE_SENDDATA:
            //    packet_id = message[0];
            //    /* sync time with upstream requests */
            //    rtcz.setEpoch(timestamp);
            //    sendDataPacket(packet_id, ++last_seq, route, route_size); // TODO: get packet # request from message
            //    return MESSAGE_CODE_SENT_NEXT_DATA_PACKET;
            //case PACKET_TYPE_DATA:
            case 2:
                collector_handleDataMessage(route[0], message, msg_size);
                return 5;
                //return MESSAGE_CODE_RECEIVED_DATA_PACKET;
            //case PACKET_TYPE_STANDBY:
            //    standby(message[0]); // up to 255 seconds. TODO, use 2 bytes for longer timeouts
            //    return MESSAGE_CODE_STANDBY;
            //case PACKET_TYPE_ECHO:
            //    if (!isCollector) {
            //        handleEchoMessage(++last_seq, route, route_size, message, msg_size);
            //    } else {
            //        println("MESSAGE:");
            //        for (uint8_t i=0; i<msg_size; i++) {
            //            print("%d ", message[i]);
            //        }
            //        println("");
            //    }
        }
    } else if (dest == 255) { // broadcast message
        //switch (packetType) {
        //    case PACKET_TYPE_STANDBY:
        //        if (!isCollector) {
        //            println("REC'd BROADCAST STANDBY FOR: %d", message[0]);
        //            routeMessage(255, last_seq, PACKET_TYPE_STANDBY, route, route_size, message, msg_size);
        //            standby(message[0]);
        //        }
        //        return MESSAGE_CODE_STANDBY;
        //}
    } else { // route this message
        //routeMessage(dest, last_seq, packetType, route, route_size, message, msg_size);
        //return MESSAGE_CODE_ROUTED;
    }
    //return MESSAGE_CODE_NONE;
    return 0;
}

void collector_onReceive(int packetSize)
{
    static uint8_t route_buffer[MAX_ROUTE_SIZE];
    static uint8_t msg_buffer[255];
    int to = LoRa.read();
    if (to != config.node_id) return;
    int from = LoRa.read();
    int dest = LoRa.read();
    int seq = LoRa.read();
    int type = LoRa.read();
    uint32_t ts = LoRa.read() << 24 | LoRa.read() << 16 | LoRa.read() << 8 | LoRa.read();
    size_t route_idx_ = 0;
    size_t msg_idx_ = 0;
    print("REC'D: TO: %d; FROM: %d; DEST: %d; SEQ: %d; TYPE: %d; RSSI: %d; ts: %u",
        to, from, dest, seq, type, LoRa.packetRssi(), ts);
    print("; ROUTE:");
    while (LoRa.available()) {
        uint8_t node = LoRa.read();
        if (node == 0) break;
        route_buffer[route_idx_++] = node;
        print(" %d", route_buffer[route_idx_-1]);
    }
    println("");
    println("SNR: %f; FRQERR: %f", LoRa.packetSnr(), LoRa.packetFrequencyError());
    while (LoRa.available()) {
        msg_buffer[msg_idx_++] = LoRa.read();
    }
    collector_handlePacket(to, from, dest, seq, type, ts, route_buffer, route_idx_, msg_buffer, msg_idx_, 0);
}

void setup()
{
    unsigned long _start = millis();
    while ( !Serial && (millis() - _start) < 10000 ) {}
    loadConfig();
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
    if (ready_to_post > 0) {
        LoRa.idle();
        digitalWrite(config.RFM95_CS, HIGH);
        SPI.endTransaction();
        sendDataToApi(ready_to_post);
        nodes_collected[ready_to_post] = 1;
        ready_to_post = 0;
        digitalWrite(config.RFM95_CS, LOW);
        /* A fresh begin seems to be required for LoRa to recover the SPI bus from WiFi */
        if (!LoRa.begin(915E6)) {
            Serial.println("LoRa init failed");
            while(true);
        }
        LoRa.receive();
        for (int i=0; i<sizeof(nodes); i++) {
            if (nodes_collected[nodes[i]] == 0) {
                sendCollectPacket(nodes[i], 0);
                return;
            }
        }
        memset(nodes_collected, 0, sizeof(nodes_collected));
        sendStandby();
    }
    if (runEvery()) sendCollectPacket(2,0);
}
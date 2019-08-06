#include <LoRa.h>
#include "config.h"
//#include "lora.h"
#include <WiFi101.h>
#include <LoRaHarvest.h>
#include <console.h>
#include <DataManager.h>

#define WIFI_CS 8
#define WIFI_IRQ 7
#define WIFI_RST 4
#define WIFI_EN 2

#define NODE_ID 1


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


WiFiClient client;
//WiFiSSLClient client;

static bool wifi_present = false;

void printWiFiStatus() {
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


void connectToServer(WiFiClient& client, char ssid[], char pass[], char host[], int port) {
    int status = WL_IDLE_STATUS;
    // client;
    WiFi.setPins(WIFI_CS, WIFI_IRQ, WIFI_RST, WIFI_EN);
    if (WiFi.status() == WL_NO_SHIELD) {
        Serial.println("WiFi shield not present");
        // don't continue
        while (true) {}
    }
    while (status!= WL_CONNECTED) {
        Serial.print("Attempting to connect to SSID: ");
        Serial.println(ssid);
        Serial.print("Using password: ");
        Serial.println(pass);
        status = WiFi.begin(ssid, pass);
        //delay(10000);  // wait 10 seconds for connection
        Serial.println("Connected to WiFi");
        printWiFiStatus();
        Serial.print("\nStarting connection to ");
        Serial.print(host); Serial.print(":"); Serial.println(port);
        if (client.connect(host, port)) {
            Serial.println("connected to server");
        } else {
            Serial.println("server connection failed");
        }
    }
}

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

void postToAPI(WiFiClient& client, Message* msg)
{
    char str[200];
    // sprintf(str,
    //    "[{\"ver\":%i,\"net\":%i,\"node\":%i,\"data\":%s}]",
    //    msg->sensorgrid_version, msg->network_id, msg->from_node, msg->data);
    sprintf(str, "%s", msg->data);
    Serial.println(str);
    Serial.print("posting message len: ");
    Serial.println(strlen(str), DEC);

    // client.println("POST /data/ HTTP/1.1");
    client.print("POST /networks/");
    client.print(config.network_id);
    client.println("/data/ HTTP/1.1");
    client.println("Host: sensorgridapi.knightlab.com");
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(strlen(str));
    client.println();
    client.println(str);
    Serial.println("Post to API completed.");
    while (!client.available()) {}
    while (client.available()) {
        char c = client.read();
        Serial.write(c);
    }
    Serial.println("");
    // client.println("Connection: close"); //close connection before sending a new request
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

void collector_handleDataMessage(uint8_t from_node, uint8_t *message, size_t msg_size)
{
    /**
     * If we asked for packet 0, then the actual packet we were waiting for is whatever
     * the node says it is.
     */
    println("Collecting packet: %d", collectingPacketId());
    if (collectingPacketId() == 0) collectingPacketId(message[0]);
    print("NODE: %d; PACKET: %d; MESSAGE:", from_node, message[0]);
    for (uint8_t i=1; i<msg_size; i++) {
        //print(" %f", (float)message[i] / 10.0);
        print("%c", message[i]);
    }
    recordData((char*)(&message[1]), msg_size-1);
    println("");
    char *batch = getCurrentBatch();
    println("** CURRENT BATCH:");
    for (int i=0; i<strlen(batch); i++) print("%c", batch[i]);
    println("");
    waitingPacket(false);
    println("collection state: collecting: %d, waiting: %d, node idx: %d, packet: %d",
        collectingData(), waitingPacket(), collectingNodeIndex(), collectingPacketId());
}

int collector_handlePacket(int to, int from, int dest, int seq, int packetType, uint32_t timestamp, uint8_t *route, size_t route_size, uint8_t *message, size_t msg_size, int topology)
{
    static int last_seq = 0;
    if (seq == last_seq) {
        return MESSAGE_CODE_DUPLICATE_SEQUENCE;
    }
    if (to != nodeId() && to != 255) return MESSAGE_CODE_WRONG_ADDRESS;
    if (!topologyTest(topology, to, from)) return MESSAGE_CODE_TOPOLOGY_FAIL;
    last_seq = seq;
    uint8_t packet_id;
    if (dest == nodeId()) {
        switch (packetType) {
            case PACKET_TYPE_SENDDATA:
                packet_id = message[0];
                /* sync time with upstream requests */
                rtcz.setEpoch(timestamp);
                sendDataPacket(packet_id, ++last_seq, route, route_size); // TODO: get packet # request from message
                return MESSAGE_CODE_SENT_NEXT_DATA_PACKET;
            case PACKET_TYPE_DATA:
                collector_handleDataMessage(route[0], message, msg_size);
                return MESSAGE_CODE_RECEIVED_DATA_PACKET;
            case PACKET_TYPE_STANDBY:
                standby(message[0]); // up to 255 seconds. TODO, use 2 bytes for longer timeouts
                return MESSAGE_CODE_STANDBY;
            case PACKET_TYPE_ECHO:
                if (!isCollector) {
                    handleEchoMessage(++last_seq, route, route_size, message, msg_size);
                } else {
                    println("MESSAGE:");
                    for (uint8_t i=0; i<msg_size; i++) {
                        print("%d ", message[i]);
                    }
                    println("");
                }
        }
    } else if (dest == 255) { // broadcast message
        switch (packetType) {
            case PACKET_TYPE_STANDBY:
                if (!isCollector) {
                    println("REC'd BROADCAST STANDBY FOR: %d", message[0]);
                    routeMessage(255, last_seq, PACKET_TYPE_STANDBY, route, route_size, message, msg_size);
                    standby(message[0]);
                }
                return MESSAGE_CODE_STANDBY;
        }
    } else { // route this message
        routeMessage(dest, last_seq, packetType, route, route_size, message, msg_size);
        return MESSAGE_CODE_ROUTED;
    }
    return MESSAGE_CODE_NONE;
}

void collector_onReceive(int packetSize)
{
    static uint8_t route_buffer[MAX_ROUTE_SIZE];
    static uint8_t msg_buffer[255];
    int to = LoRa.read();
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
    // Serial.begin(9600);
    delay(10000);
    loadConfig();
    Serial.println("Config loaded");
    // delay(1000);
    //setup_radio(config.RFM95_CS, config.RFM95_INT, config.node_id);
    nodeId(NODE_ID);
    if (nodeId() == 1) isCollector = true;
    println("Configuring LoRa with pins: CS: %d; RST: %d; IRQ: %d",
        config.RFM95_CS, config.RFM95_RST, config.RFM95_INT);
    LoRa.setPins(config.RFM95_CS, config.RFM95_RST, config.RFM95_INT);
    if (!LoRa.begin(915E6)) {
        //print("version: %x", LoRa.readRegister(0x42));
        Serial.println("LoRa init failed");
        while(true);
    }
    LoRa.enableCrc();
    LoRa.onReceive(onReceive);
    LoRa.receive();
    rtcz.begin();
    /** setup wifi **/
    if (config.wifi_password) {
        Serial.print("Attempting to connect to Wifi: ");
        Serial.print(config.wifi_ssid);
        Serial.print(" with password: ");
        Serial.println(config.wifi_password);
        connectToServer(client, config.wifi_ssid, config.wifi_password, config.api_host, config.api_port);
        WiFi.maxLowPowerMode();
        wifi_present = true;
        //client.stop();
        WiFi.end();
        //WiFi.disconnect();
    } else {
        Serial.println("No WiFi configuration found");
    }
}

void sendDataToApi()
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
    if (wifi_present) {
        Serial.print("Attempting to connect to Wifi: ");
        Serial.print(config.wifi_ssid);
        Serial.print(" with password: ");
        Serial.println(config.wifi_password);
        connectToServer(client, config.wifi_ssid, config.wifi_password, config.api_host, config.api_port);
        //WiFi.maxLowPowerMode();
        client.print("POST /networks/");
        //client.print(config.network_id);
        client.print(1);
        client.print("/nodes/");
        //client.print(nodes[collectingNodeIndex()]);
        client.print(2);
        client.println("/data/ HTTP/1.1");
        client.println("Host: sensorgridapi.knilab.com");
        client.println("Content-Type: application/json");
        client.print("Content-Length: ");
        client.println(msg_length);
        client.println();
    }
    client.print("{ \"data\": [");
    for (int i=0; i<numBatches(0); i++) {
        char *batch = (char*)getBatch(i);
        print(batch);
        client.print(batch);
        if (i < numBatches(0)-1) {
            client.print(",");
        }
    }
    client.println("]}");
    while (!client.available()) {}
    while (client.available()) {
        char c = client.read();
        Serial.write(c);
    }
    println("**********");
    if (wifi_present){
        WiFi.end();
    }
    clearData();
    collectingNodeIndex(collectingNodeIndex() + 1);
}

void tick()
{
    static unsigned long tick_time = 0;
    if (millis() > tick_time) {
        print(".");
        tick_time = millis() + 1000;
    }
}

static uint8_t seq = 0;

void sendCollectPacket(uint8_t node_id, uint8_t packet_id)
{
    println("prefetch collection state: collecting: %d, waiting: %d, node idx: %d, packet: %d",
        collectingData(), waitingPacket(), collectingNodeIndex(), packet_id);
    waitingPacket(true);
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
    LoRa.write(nodeId());
    LoRa.write(node_id);
    LoRa.write(++++seq);
    LoRa.write(PACKET_TYPE_SENDDATA);
    writeTimestamp();
    LoRa.write(route, route_size);
    LoRa.write(0); // end route
    LoRa.write(packet_id); // packet id
    LoRa.endPacket();
    //timeout = millis() + 10000;
    //println("set timeout to: %d", timeout);
    LoRa.receive();
}

void loop() {
    tick();
    static unsigned long timeout = 0;
    if (isCollector && runEvery()) collectingData(true);
    if (collectingData()) {
        if (waitingPacket()) {
            if (millis() > timeout) {
                println("TIMEOUT");
                collectingNodeIndex(-1);
                collectingPacketId(1);
                collectingData(false); // TODO: retry data fetch
                waitingPacket(false);
            }
        } else {
            collectingPacketId(collectingPacketId() - 1);
            if (collectingPacketId() == 0) {
                sendDataToApi();
                /*
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
                if (wifi_present) {
                    Serial.print("Attempting to connect to Wifi: ");
                    Serial.print(config.wifi_ssid);
                    Serial.print(" with password: ");
                    Serial.println(config.wifi_password);
                    connectToServer(client, config.wifi_ssid, config.wifi_password, config.api_host, config.api_port);
                    //WiFi.maxLowPowerMode();
                    client.print("POST /networks/");
                    //client.print(config.network_id);
                    client.print(1);
                    client.print("/nodes/");
                    //client.print(nodes[collectingNodeIndex()]);
                    client.print(2);
                    client.println("/data/ HTTP/1.1");
                    client.println("Host: sensorgridapi.knilab.com");
                    client.println("Content-Type: application/json");
                    client.print("Content-Length: ");
                    client.println(msg_length);
                    client.println();
                }
                client.print("{ \"data\": [");
                for (int i=0; i<numBatches(0); i++) {
                    char *batch = (char*)getBatch(i);
                    print(batch);
                    client.print(batch);
                    if (i < numBatches(0)-1) {
                        client.print(",");
                    }
                }
                client.println("]}");
                while (!client.available()) {}
                while (client.available()) {
                    char c = client.read();
                    Serial.write(c);
                }
                println("**********");
                if (wifi_present){
                    WiFi.end();
                }
                clearData();
                collectingNodeIndex(collectingNodeIndex() + 1);
                */
            }
            if (collectingNodeIndex() >= sizeof(nodes)) {
                for (int i=0; i<sizeof(nodes); i++) {
                    LoRa.flush();
                    LoRa.idle();
                    LoRa.beginPacket();
                    LoRa.write(routes[nodes[i]][1]);
                    LoRa.write(nodeId());
                    LoRa.write(nodes[i]);
                    LoRa.write(++++seq);
                    LoRa.write(PACKET_TYPE_STANDBY);
                    writeTimestamp();
                    LoRa.write(routes[nodes[i]], sizeof(routes[nodes[i]]));
                    LoRa.write(0); // end route
                    LoRa.write(nextCollection() / 1000);
                    LoRa.endPacket();
                    LoRa.receive();
                }
                collectingNodeIndex(-1);
                collectingPacketId(1);
                collectingData(false);
                waitingPacket(false);
                return;
            }

            uint8_t node_id = nodes[collectingNodeIndex()];
            sendCollectPacket(node_id, collectingPacketId());
            /*
            println("prefetch collection state: collecting: %d, waiting: %d, node idx: %d, packet: %d",
                collectingData(), waitingPacket(), collectingNodeIndex(), collectingPacketId());
            waitingPacket(true);
            uint8_t node_id = nodes[collectingNodeIndex()];
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
            LoRa.write(nodeId());
            LoRa.write(node_id);
            LoRa.write(++++seq);
            LoRa.write(PACKET_TYPE_SENDDATA);
            writeTimestamp();
            LoRa.write(route, route_size);
            LoRa.write(0); // end route
            LoRa.write(collectingPacketId()); // packet id
            LoRa.endPacket();
            timeout = millis() + 10000;
            println("set timeout to: %d", timeout);
            LoRa.receive();
            */

        }
    }
}
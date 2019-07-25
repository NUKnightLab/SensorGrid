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
        delay(10000);  // wait 10 seconds for connection
        Serial.println("Connected to WiFi");
        printWiFiStatus();
        Serial.println("\nStarting connection to server...");
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

void setup()
{
    // Serial.begin(9600);
    delay(10000);
    loadConfig();
    Serial.println("Config loaded");
    // delay(1000);
    //setup_radio(config.RFM95_CS, config.RFM95_INT, config.node_id);
    if (config.wifi_password) {
        Serial.print("Attempting to connect to Wifi: ");
        Serial.print(config.wifi_ssid);
        Serial.print(" with password: ");
        Serial.println(config.wifi_password);
        connectToServer(client, config.wifi_ssid, config.wifi_password, config.api_host, config.api_port);
        WiFi.maxLowPowerMode();
        wifi_present = true;
    } else {
        Serial.println("No WiFi configuration found");
    }
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
}

void _loop()
{
    static int i = 0;
    Serial.println(i++, DEC);
    //static uint8_t buf[RH_ROUTER_MAX_MESSAGE_LEN];
    //static Message msg[sizeof(Message)] = {0};
    //static Message *msg = (Message*)buf;
 
    /*
    Message msg = {
        .sensorgrid_version=config.sensorgrid_version,
        .network_id=config.network_id,
        .from_node=config.node_id,
        .message_type=1,
        .len=0
    };
    */
    // uint8_t *_msg = (uint8_t*)&msg;
    // uint8_t len = sizeof(msg);
    // uint8_t toID = 2;
    // send_message((uint8_t*)&msg, len, toID);

/*
    if (RECV_STATUS_SUCCESS == receive(msg, 60000)) {
        Serial.println("Received message");
        Serial.print("VERSION: "); Serial.println(msg->sensorgrid_version, DEC);
        Serial.print("DATA: ");
        Serial.println((char*)msg->data);
        for (int i=0; i < msg->len; i++) print("%s", msg->data[i]);
        Serial.println("");
        Serial.print("WiFi Status");
        printWiFiStatus();
        if (wifi_present) {
            if (WiFi.status() == WL_CONNECTED) {
                while (!client.connected()) {
                    Serial.println("Reconnecting wifi");
                    reconnectClient(client, config.wifi_ssid);
                }
            } else {
                connectToServer(client, config.wifi_ssid, config.wifi_password, config.api_host, config.api_port);
            }
            Serial.println("Posting to API");
            postToAPI(client, msg);
        } else {
            Serial.println("Collector Node with no WiFi configuration. Assuming serial collection");
        }
    } else {
        Serial.println("No message received ");
    }
  */

    // delay(10000);
}

void loop() {
    static unsigned long tick_time = 0;
    if (millis() > tick_time) {
        print(".");
        tick_time = millis() + 1000;
    }

    static unsigned long timeout = 0;
    static uint8_t seq = 0;
    if (isCollector && runEvery()) collectingData(true);
    if (!isCollector && scheduleDataSample(5000)) recordBattery();
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
                // send collected node data to the API
                println("***** SEND DATA TO API *****");
                print("{ node: %d, data: [", nodes[collectingNodeIndex()]);
                for (int i=0; i<numBatches(0); i++) {
                    char *batch = (char*)getBatch(i);
                    print(batch);
                    //for (int j=0; j<10; j++) print("%c", batch[j]);
                    if (i < numBatches(0)-1) print(",");
                }
                println("]}");
                println("**********");
                clearData();
                collectingNodeIndex(collectingNodeIndex() + 1);
            }
            if (collectingNodeIndex() >= sizeof(nodes)) {
                for (int i=0; i<sizeof(nodes); i++) {
                    // send shutdown
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
                // TODO: send standby
                return;
            }
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
            //Serial.println("Sending broadcast standby");
            //LoRa.idle();
            //LoRa.beginPacket();
            //LoRa.write(255);
            //LoRa.write(NODE_ID);
            //LoRa.write(255);
            //LoRa.write(++++seq);
            //LoRa.write(PACKET_TYPE_STANDBY);
            //LoRa.write(0);
            //LoRa.write(20); // 20 seconds
            //LoRa.endPacket();
            //LoRa.receive();
        }
    }
}
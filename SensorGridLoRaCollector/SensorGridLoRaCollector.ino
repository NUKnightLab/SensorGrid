#include "config.h"
#include <KnightLab_LoRa.h>
#include <WiFi101.h>

#define WIFI_CS 8
#define WIFI_IRQ 7
#define WIFI_RST 4
#define WIFI_EN 2

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

void setup()
{
    // Serial.begin(9600);
    delay(10000);
    loadConfig();
    Serial.println("Config loaded");
    // delay(1000);
    setupLoRa(config.node_id, config.RFM95_CS, config.RFM95_INT, config.tx_power);

    return; // skipping wifi for now
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
}

void loop()
{
    static uint8_t buf[KL_ROUTER_MAX_MESSAGE_LEN];
    //static Message msg[sizeof(Message)] = {0};
    static Message *msg = (Message*)buf;
    static int counter = 0;
    static unsigned long beacon_time = 0;
    static unsigned long data_request_time = 0;
    static bool have_routes = false;
    uint8_t len = sizeof(buf);
    uint8_t source;
    uint8_t dest;
    uint8_t msg_id;
    uint8_t flags;

    if (++counter % 10000 == 0)
        Serial.print(".");
    if (counter % 1000000 == 0)
        Serial.println("");

    if (LoRaRouter->recvfromAck(buf, &len, &source, &dest, &msg_id, &flags)) {
        // received a message
    }

    if (LoRaRouter->routingTableIsEmpty()) {
        if (have_routes) {
            beacon_time = millis() + random(1000, 2000);
            have_routes = false;
        }
    } else {
        have_routes = true;
    }

    if (millis() > beacon_time) {
        Serial.println("BEACON DUE");
        if (!have_routes) {
            Serial.println("HAVE NO ROUTES. SENDING BEACON");
            LoRaRadio->waitPacketSent();
            LoRaRadio->setModeIdle();
            if (LoRaRadio->isChannelActive()) {
                return;
            }
            uint8_t dest = 0;
            uint8_t resp = LoRaRouter->doArp(0);
            Serial.print("RECEVIED: ");
            Serial.println(resp);
        }
        beacon_time = millis() + 10000;
    }

    if (millis() > data_request_time) {
        Serial.println("SCHEDULED DATA TRANSMISSION REQUEST DUE");
        if (!have_routes) {
            Serial.println("HAVE NO ROUTES. NOT REQUESTING DATA");
        } else {
            uint8_t msg[] = "SEND DATA";
            uint8_t len = sizeof(msg);
            uint8_t dest = 2;
            uint8_t resp = 1;
            while (resp > 0) {
                resp = LoRaRouter->sendtoWait(msg, len, dest);
                if (!resp) break; // 0 is success
                // We need to keep tryin until the nodes are in their data tx cycle. TODO: should we
                // sync up schedules instead of this brute force repeat?
                Serial.println("No response. Waiting 10 seconds ..");
                delay(10000);
            }
            len = sizeof(buf);
            uint8_t from;
            uint8_t to;
            uint8_t msg_id;
            uint8_t flags = KL_FLAGS_MORE_DATA;
            // TODO: receive data until we get a no-more-data flag
            Serial.println("RECEIVING DATA FROM REMOTE");
            while ( (flags & KL_FLAGS_MORE_DATA) == KL_FLAGS_MORE_DATA) {
                if ( LoRaRouter->recvfromAck(buf, &len, &from, &to, &msg_id, &flags) ) {
                    Serial.print("Message received: ");
                    Serial.println((char*)buf);
                    Serial.print("With flags: ");
                    Serial.println(flags);
                }
            }
        }
        data_request_time = millis() + 30000;
    }


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

#include "config.h"
#include "lora.h"
#include <WiFi101.h>

WiFiClient client;

void postToAPI(WiFiClient& client, Message* msg)
{
    char str[200];
    sprintf(str,
        "{\"ver\":%i,\"net\":%i,\"orig\":%i,\"id\":%i,\"data\":%s}",
        msg->sensorgrid_version, msg->network_id, msg->from_node, 0, msg->data);
    Serial.println(str);
    /*
    client.println("POST /data HTTP/1.1");
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(strlen(str));
    client.println();
    client.println(str);
    Serial.println("Post to API completed.");
    while (client.available()) {
        char c = client.read();
        Serial.write(c);
    }
    client.println("Connection: close"); //close connection before sending a new request
    */
}

void setup()
{
    //Serial.begin(9600);
    delay(10000);
    loadConfig();
    Serial.println("Config loaded");
    //delay(1000);
    setup_radio(config.RFM95_CS, config.RFM95_INT, config.node_id);
}

void loop()
{
    static int i = 0;
    Serial.println(i++, DEC);
    static uint8_t buf[RH_ROUTER_MAX_MESSAGE_LEN];
    //static Message msg[sizeof(Message)] = {0};
    static Message *msg = (Message*)buf;
    
    /*
    Message msg = {
        .sensorgrid_version=config.sensorgrid_version,
        .network_id=config.network_id,
        .from_node=config.node_id,
        .message_type=1,
        .len=0
    };
    */
    //uint8_t *_msg = (uint8_t*)&msg;
    //uint8_t len = sizeof(msg);
    //uint8_t toID = 2;
    //send_message((uint8_t*)&msg, len, toID);

    if (RECV_STATUS_SUCCESS == receive(msg, 60000)) {
        Serial.println("Received message");
        Serial.print("VERSION: "); Serial.println(msg->sensorgrid_version, DEC);
        Serial.print("DATA: ");
        Serial.println((char*)msg->data);
        postToAPI(client, msg);
    } else {
        Serial.println("No message received ");
    }

    //delay(10000);
}

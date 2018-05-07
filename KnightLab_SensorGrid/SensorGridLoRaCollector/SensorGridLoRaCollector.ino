#include "config.h"
#include "lora.h"

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
    static uint8_t buf[140] = {0};
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
        Serial.println("DATA: ");
        logln((char*)(msg->data));
    } else {
        Serial.println("No message received ");
    }

    //delay(10000);
}

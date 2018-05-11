#include "lora.h"

RH_RF95 *radio;
#if defined(USE_MESH_ROUTER)
RHMesh* router;
#else
RHRouter* router;
#endif

void setup_radio(uint8_t cs_pin, uint8_t int_pin, uint8_t node_id)
{
    logln(F("Setting up radio with RadioHead Version: %d.%d"),
        RH_VERSION_MAJOR, RH_VERSION_MINOR);
    /* TODO: Can RH version check be done at compile time? */
    if (RH_VERSION_MAJOR != REQUIRED_RH_VERSION_MAJOR
            || RH_VERSION_MINOR != REQUIRED_RH_VERSION_MINOR) {
        //log(F("ABORTING: SensorGrid requires RadioHead version %s.%s"),
        //    REQUIRED_RH_VERSION_MAJOR, REQUIRED_RH_VERSION_MINOR);
        //log(F("RadioHead %s.%s is installed"), RH_VERSION_MAJOR, RH_VERSION_MINOR);
        while(1);
    }
    radio = new RH_RF95(cs_pin, int_pin);
#if defined(USE_MESH_ROUTER)
    router = new RHMesh(*radio, node_id);
#else
    router = new RHRouter(*radio, NODE_ID);
    /* RadioHead sometimes continues retrying transmissions even after a
       successful reliable delivery. This seems to persist even when switching
       to a continually listening mode, effectively flooding the local
       air space while trying to listen for new messages. As a result, don't
       do any retries in the router. Instead, we will pick up missed messages
       in the application layer. */
    //router->setRetries(0);
    router->clearRoutingTable();
    //router->addRouteTo(1, 1);
    //router->addRouteTo(2, 2);
    //router->addRouteTo(3, 3);
    //router->addRouteTo(4, 4);
#endif
    if (USE_SLOW_RELIABLE_MODE)
        radio->setModemConfig(RH_RF95::Bw125Cr48Sf4096);
    logln(F("Node ID: %d"), node_id);
    if (!router->init()) {
        logln(F("Router init failed"));
        while(1);
    }
    logln(F("FREQ: %s"), FREQ);
        if (!radio->setFrequency(FREQ)) {
        logln(F("Radio frequency set failed"));
        while(1);
    }
    radio->setTxPower(TX, false);
    radio->setCADTimeout(CAD_TIMEOUT);
    /* TODO: what is the correct timeout? From the RH docs:
[setTimeout] Sets the minimum retransmit timeout. If sendtoWait is waiting
for an ack longer than this time (in milliseconds), it will retransmit the
message. Defaults to 200ms. The timeout is measured from the end of
transmission of the message. It must be at least longer than the the transmit
time of the acknowledgement (preamble+6 octets) plus the latency/poll time of
the receiver. */
    router->setTimeout(ROUTER_TIMEOUT);
    delay(100);
}

/* **** RECEIVE FUNCTIONS **** */

int receive(Message *msg, uint16_t timeout)
{
    uint8_t len = RH_ROUTER_MAX_MESSAGE_LEN;
    uint8_t from;
    uint8_t to;
    uint8_t id;
    memset(msg->data, 0, 100);
    logln(F("Listening for message ..."));
    if (router->recvfromAckTimeout((uint8_t*)msg, &len, timeout, &from, &to, &id)) {
        if (msg->sensorgrid_version != config.sensorgrid_version) {
            logln(F("WARNING: Received message with wrong firmware version: %d"),
                msg->sensorgrid_version);
            return RECV_STATUS_WRONG_VERSION;
        }
        if (msg->network_id != config.network_id) {
            logln(F("WARNING: Received message from wrong network: %d (expected: %d)"),
                msg->network_id, config.network_id);
            return RECV_STATUS_WRONG_NETWORK;
        }
        Serial.print("Received message len: ");
        Serial.println(len, DEC);
        uint8_t* buf = (uint8_t*)msg;
        for (int i=0; i<len; i++) {
            Serial.print( buf[i], HEX); Serial.print(" ");
        }
        Serial.println("");
        Serial.print("version: "); Serial.println(msg->sensorgrid_version);
        Serial.print("nework: "); Serial.println(msg->network_id);
        Serial.print("from: "); Serial.println(msg->from_node);
        Serial.print("type: "); Serial.println(msg->message_type);
        Serial.print("len (0): "); Serial.println(msg->len);
        log_("data: ");
        println(msg->data);
        return RECV_STATUS_SUCCESS;
    } else {
        return RECV_STATUS_NO_MESSAGE;
    }
}

/* **** SEND FUNCTIONS **** */

uint8_t send_message(uint8_t* msg, uint8_t len, uint8_t toID)
{
    //log_(F("Sending message type: "));
    //print_message_type(((Message*)msg)->message_type);
    //p(F("; length: %d\n"), len);
    unsigned long start = millis();
    uint8_t err = router->sendtoWait(msg, len, toID);

    /* TODO: sleep
    if (millis() < next_listen) {
        Serial.println("Listen timeout not expired. Sleeping.");
        radio.sleep();
    }
    */

    //p(F("Time to send: %d\n"), millis() - start);
    if (err == RH_ROUTER_ERROR_NONE) {
        return err;
    } else if (err == RH_ROUTER_ERROR_INVALID_LENGTH) {
        logln(F("ERROR sending message to Node ID: %d. INVALID LENGTH"), toID);
        return err;
    } else if (err == RH_ROUTER_ERROR_NO_ROUTE) {
        logln(F("ERROR sending message to Node ID: %d. NO ROUTE"), toID);
        return err;
    } else if (err == RH_ROUTER_ERROR_TIMEOUT) {
        logln(F("ERROR sending message to Node ID: %d. TIMEOUT"), toID);
        return err;
    } else if (err == RH_ROUTER_ERROR_NO_REPLY) {
        logln(F("ERROR sending message to Node ID: %d. NO REPLY"), toID);
        return err;
    } else if (err == RH_ROUTER_ERROR_UNABLE_TO_DELIVER) {
        logln(F("ERROR sending message to Node ID: %d. UNABLE TO DELIVER"), toID);
        return err;   
    } else {
        logln(F("ERROR sending message to Node ID: %d. UNKOWN ERROR CODE: %d"), toID, err);
        return err;
    }
}
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


int receive(Message *msg, uint16_t timeout)
{
    //static uint8_t recv_buf[RH_ROUTER_MAX_MESSAGE_LEN];
    static uint8_t len = RH_ROUTER_MAX_MESSAGE_LEN;
    static uint8_t from;
    static uint8_t to;
    static uint8_t id;
    logln("Listening for message ...");
    if (router->recvfromAckTimeout((uint8_t*)msg, &len, timeout, &from, &to, &id)) {
        //Message *_msg = (Message*)recv_buf;
        if ( msg->sensorgrid_version != config.sensorgrid_version ) {
            logln(F("WARNING: Received message with wrong firmware version: %d\n"),
                msg->sensorgrid_version);
            return RECV_STATUS_WRONG_VERSION;
        }           
        if ( msg->network_id != config.network_id ) {
            logln(F(
                "WARNING: Received message from wrong network: %d (expected: %d)\n"),
                msg->network_id, config.network_id);
            return RECV_STATUS_WRONG_NETWORK;
        }
        //validate_recv_buffer(*len);
        logln(F("Received buffered message. len: %d; type: %d"), len,
            msg->message_type);
            // get_message_type(_msg->message_type));
        logln(F("; from: %d; rssi: %d\n"), from, radio->lastRssi());
        return RECV_STATUS_SUCCESS;
        //memcpy(msg, recv_buf, len);
        //last_rssi[*source] = radio->lastRssi();
        //return _msg->message_type;
    } else {
        return RECV_STATUS_NO_MESSAGE;
    }
}

/* **** SEND FUNCTIONS **** */
uint8_t send_message(Message *msg, uint8_t len, uint8_t to_id)
{
    uint8_t err = router->sendtoWait((uint8_t*)msg, len, to_id);
    if (err == RH_ROUTER_ERROR_NONE) {
        return err;
    } else if (err == RH_ROUTER_ERROR_INVALID_LENGTH) {
        logln(F("ERROR sending message to node ID: %d. INVALID LENGTH"), to_id);
        return err;
    } else if (err == RH_ROUTER_ERROR_NO_ROUTE) {
        logln(F("ERROR sending message to node ID: %d. NO ROUTE"), to_id);
        return err;
    } else if (err == RH_ROUTER_ERROR_TIMEOUT) {
        logln(F("ERROR sending message to node ID: %d. TIMEOUT"), to_id);
        return err;
    } else if (err == RH_ROUTER_ERROR_NO_REPLY) {
        logln(F("ERROR sending message to node ID: %d. NO REPLY"), to_id);
        return err;
    } else if (err == RH_ROUTER_ERROR_UNABLE_TO_DELIVER) {
        logln(F("ERROR sending message to node ID: %d. UNABLE TO DELIVER"), to_id);
        return err;
    }
}
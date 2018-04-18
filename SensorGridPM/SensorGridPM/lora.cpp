#include "lora.h"

RH_RF95 *radio;
#if defined(USE_MESH_ROUTER)
RHMesh* router;
#else
RHRouter* router;
#endif

void setup_radio()
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
    radio = new RH_RF95(RFM95_CS, RFM95_INT);
#if defined(USE_MESH_ROUTER)
    router = new RHMesh(*radio, NODE_ID);
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
    logln(F("Node ID: %d"), NODE_ID);
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

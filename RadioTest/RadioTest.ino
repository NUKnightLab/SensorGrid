#include <RHMesh.h>
#include <RHRouter.h>
#include <RH_RF95.h>
#define NODE_ID 14
#define FREQ 915.00
#define TX 5
#define CAD 2000
#define TIMEOUT 1000
#define CS 8
#define INT 3

static RHMesh* router;
RH_RF95 *radio;


void setupRadio(RH_RF95 rf95)
{
    Serial.print("Setting up radio with RadioHead Version ");
    Serial.print(RH_VERSION_MAJOR, DEC); Serial.print(".");
    Serial.println(RH_VERSION_MINOR, DEC);
    Serial.print("Node ID: ");
    Serial.println(NODE_ID);
    router = new RHMesh(rf95, NODE_ID); 
    //rf95.setModemConfig(RH_RF95::Bw125Cr48Sf4096); // doesn't work with RH 1.71
    if (!router->init())
        Serial.println("Router init failed");
    Serial.print(F("FREQ: ")); Serial.println(FREQ);
    if (!rf95.setFrequency(FREQ)) {
        Serial.println("Radio frequency set failed");
    } 
    rf95.setTxPower(TX, false);
    //rf95.setCADTimeout(CAD);
    router->setTimeout(TIMEOUT);
    delay(100);
}


void setup() {
    while (!Serial);
    radio = new RH_RF95(CS, INT);
    setupRadio(*radio);
}

void loop() {
  uint8_t data[] = "Message";
  bool collector = false;
  if (collector) {
    Serial.println("Sending message");
    if (router->sendtoWait(data, sizeof(data), 14) != RH_ROUTER_ERROR_NONE) {
        Serial.println("sendtoWait failed");
    }
    delay(10000);
  } else {
    uint8_t buf[RH_MESH_MAX_MESSAGE_LEN];
    uint8_t from;
    uint8_t len = sizeof(buf);
    Serial.println("Waiting for request");
    if (router->recvfromAckTimeout(data, &len, 5000, &from)) {
        Serial.print("Request from: ");
        Serial.println(from, DEC);
    }
  }
}

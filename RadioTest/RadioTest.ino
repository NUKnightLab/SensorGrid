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

#define CLIENT_ADDRESS 1
#define SERVER_ADDRESS 2

//static RHMesh* router;
static RHRouter* router;
RH_RF95 *radio;
bool client = true; //client node


void setupRadio(RH_RF95 rf95)
{
    Serial.print("Setting up radio with RadioHead Version ");
    Serial.print(RH_VERSION_MAJOR, DEC); Serial.print(".");
    Serial.println(RH_VERSION_MINOR, DEC);
    Serial.print("Node ID: ");
    Serial.println(NODE_ID);
    //router = new RHMesh(rf95, NODE_ID); 

    uint8_t thisAddr;
    if (client) {
      thisAddr = CLIENT_ADDRESS;
    }
    else {
      thisAddr = SERVER_ADDRESS;
    }
    
    router = new RHRouter(rf95, thisAddr);
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

    //manually add routes
    router->addRouteTo(CLIENT_ADDRESS, CLIENT_ADDRESS);
    router->addRouteTo(SERVER_ADDRESS, SERVER_ADDRESS);
    delay(100);
}

uint8_t buf[RH_MESH_MAX_MESSAGE_LEN];

void setup() {
    while (!Serial);
    radio = new RH_RF95(CS, INT);
    setupRadio(*radio);
}

void loop() {
  uint8_t data[] = "Message";
  bool collector = true;
  if (collector) {
    Serial.println("Sending message");
    uint8_t len = sizeof(buf);
    uint8_t from;
    if (router->sendtoWait(data, sizeof(data), CLIENT_ADDRESS) != RH_ROUTER_ERROR_NONE) {
        Serial.println("sendtoWait failed");
    }
    else {
      if (router->recvfromAckTimeout(buf, &len, 5000, &from)) {
        Serial.print("got reply from : 0x");
        Serial.print(from, HEX);
        Serial.print(": ");
        Serial.println((char*)buf);
      }
      else {
        Serial.println("No reply, is the server running?");
      }
    }
    delay(10000);
  } else {
    uint8_t from;
    uint8_t len = sizeof(buf);
    Serial.println("Waiting for request");

    if (router->recvfromAck(buf, &len, &from)) {
        Serial.print("got reply from : 0x");
        Serial.print(from, HEX);
        Serial.print(": ");
        Serial.println((char*)buf);

        //wait for ACK to be sent to client
        if (router->sendtoWait(data, sizeof(data), SERVER_ADDRESS) != RH_ROUTER_ERROR_NONE) {
          Serial.println("Server send to wait failed...");
        }
    }
    else {
      Serial.println("recvfromAck failed. ACK not sent to client");
    }
  }
}

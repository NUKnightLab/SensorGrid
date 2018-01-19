#include <RHMesh.h>
#include <RHRouter.h>
#include <RH_RF95.h>
#include <SPI.h>
#define FREQ 915.00
#define TX 5
#define CAD 2000
#define TIMEOUT 1000
#define CS 8
#define INT 3
#define COLLECTOR 14
#define SENSOR 3
#define NODE_TYPE SENSOR //COLLECTOR

static RHMesh* router;
//RH_RF95 radio(CS, INT);
RH_RF95 *radio;


/*
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
*/
unsigned checksum(void *buffer, size_t len, unsigned int seed)
{
      unsigned char *buf = (unsigned char *)buffer;
      size_t i;

      for (i = 0; i < len; ++i)
            seed += (unsigned int)(*buf++);
      return seed;
}

/*
struct node {
  int data;
  struct node *next;
};
node* head = (struct node*)malloc(sizeof(struct node));
head->data = 1;
head->next = NULL;
*/

void setup() {
  /*
    while (sizeof(&head) < 255) { //create linked list of size 255 to check max payload
      typedef struct node* temp = (struct node*)malloc(sizeof(struct node));
      temp->data = 1;
      struct node* curr = head;
      while (curr->next != NULL) {
        curr = curr->next;
      }
      curr->next = temp;
      temp->next = NULL;
    } */

    while (!Serial);
    Serial.print("Setting up radio with RadioHead Version ");
    Serial.print(RH_VERSION_MAJOR, DEC); Serial.print(".");
    Serial.println(RH_VERSION_MINOR, DEC);
    Serial.print("Node ID: ");
    Serial.println(NODE_TYPE);
    radio = new RH_RF95(CS, INT);
    router = new RHMesh(*radio, NODE_TYPE);
    if (!router->init())
        Serial.println("Router init failed");
    Serial.print(F("FREQ: ")); Serial.println(FREQ);
    if (!radio->setFrequency(FREQ)) {
        Serial.println("Radio frequency set failed");
    } 
    radio->setTxPower(TX, false);
    //rf95.setCADTimeout(CAD);
    router->setTimeout(TIMEOUT);
    delay(100);
}

int sensorArray[2] = {2,3};
int i=1;

void loop() {
  unsigned int checkSize = 0;
  unsigned int beforeSending = 0;
  uint8_t data[] = "Message";
  uint8_t dataSize = sizeof(data);
  Serial.print("Size of data: ");
  Serial.println(dataSize);
  uint8_t from;
  unsigned long start = 0;
  float duration = 0;
  beforeSending += checksum(0, dataSize, beforeSending);
  Serial.print("Checksum before sending message ");
  Serial.println(beforeSending);
  i!=i;
  
  if (NODE_TYPE == COLLECTOR) {
    if (router->sendtoWait(data, sizeof(data), i) != RH_ROUTER_ERROR_NONE) {
      if (router->recvfromAck(data, &dataSize, &from)) {
        Serial.println("Sending acknowledgment...");
        if (router->sendtoWait(data, sizeof(data), (int)from) != RH_ROUTER_ERROR_NONE) {
          Serial.println("Failed to resend message...");
        }
      else {
        Serial.println("Resending message...");
        }
      }
    else {
      Serial.println("receive from ack failed");
        } 
      }
    }
  else if (NODE_TYPE == SENSOR) {
    Serial.println("Sending message...");
    start = millis();
    bool wait = true;
    while (wait) {
    if (router->recvfromAck(data, &dataSize, &from)) {
      Serial.println("Received notification to send message");
      wait = false;
      }
    }
    if (router->sendtoWait(data, sizeof(data), 14) != RH_ROUTER_ERROR_NONE) {
      Serial.println("sendtoWait failed");
    }
    else {
      bool wait = true;
      while (wait) {
      if (router->recvfromAck(data, &dataSize, &from)) {
        Serial.println("Message returned from sensor node");
        wait = false;
        }
      }
      radio->sleep();
      delay(1000);
      duration = millis() - start;
      Serial.print("Time it took to send: ");
      Serial.println(duration);
      duration = 0;
      checkSize += checksum(0, dataSize, checkSize);
      Serial.print("Printing checkSize: ");
      Serial.println(checkSize);
    }
  }
delay(1000);
}
  

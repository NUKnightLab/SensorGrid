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
//#define NODE_TYPE COLLECTOR

static RH_RF95 radio(CS, INT);
static RHMesh* router;
//RH_RF95 radio(CS, INT);
//RH_RF95 *radio;

int sensorArray[2] = {2,3};

typedef struct Data {
    uint8_t id; // 1-255 indicates Data$
    uint8_t node_id;
    uint8_t timestamp;
    uint8_t type;
    int16_t value;
} data;

uint8_t recv_buf[sizeof(Data)] = {0};
int i=1;

static void clear_recv_buffer()
{
    memset(recv_buf, 0, sizeof(Data));
}

unsigned long hash(unsigned char *str)
{
    unsigned long hash = 5381;
    int c;
    while (c = *str++)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    return hash;
}

unsigned long hash2(uint8_t* msg, uint8_t len)
{
    unsigned long hash = 5381;
    int c;
    for (int i=0; i++; i<len) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

bool send_message(uint8_t* msg, uint8_t toID)
{
    Serial.print("Sending message size: ");
    Serial.print(sizeof(msg), DEC);
    Serial.print(" hash: ");
    Serial.println(hash(msg));
    Serial.print("Message ID: ");
    Serial.println( ((struct Data*)msg)->id);
    unsigned long start = millis();
    uint8_t err = router->sendtoWait(msg, sizeof(Data), toID);
    Serial.print("Time to send: ");
    Serial.println(millis() - start);
    if (err == RH_ROUTER_ERROR_NONE) {
        return true;
    } else if (err == RH_ROUTER_ERROR_INVALID_LENGTH) {
        Serial.print(F("Error receiving data from Node ID: "));
        Serial.print(toID);
        Serial.println(". INVALID LENGTH");
    } else if (err == RH_ROUTER_ERROR_NO_ROUTE) {
        Serial.print(F("Error receiving data from Node ID: "));
        Serial.print(toID);
        Serial.println(". NO ROUTE");
    } else if (err == RH_ROUTER_ERROR_TIMEOUT) {
        Serial.print(F("Error receiving data from Node ID: "));
        Serial.print(toID);
        Serial.println(". TIMEOUT");
    } else if (err == RH_ROUTER_ERROR_NO_REPLY) {
        Serial.print(F("Error receiving data from Node ID: "));
        Serial.print(toID);
        Serial.println(". NO REPLY");
    } else if (err == RH_ROUTER_ERROR_UNABLE_TO_DELIVER) {
        Serial.print(F("Error receiving data from Node ID: "));
        Serial.print(toID);
        Serial.println(". UNABLE TO DELIVER");    
    } else {
        Serial.print(F("Error receiving data from Node ID: "));
        Serial.print(toID);
        Serial.print(". UNKNOWN ERROR CODE: ");
        Serial.println(err, DEC);
    }
}
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
    //RH_RF95 radio = new RH_RF95(CS, INT);
    //RH_RF95 radio(CS, INT);
    router = new RHMesh(radio, NODE_TYPE);
    if (!router->init())
        Serial.println("Router init failed");
    Serial.print(F("FREQ: ")); Serial.println(FREQ);
    if (!radio.setFrequency(FREQ)) {
        Serial.println("Radio frequency set failed");
    } 
    radio.setTxPower(TX, false);
    //rf95.setCADTimeout(CAD);
    router->setTimeout(TIMEOUT);
    delay(100);
}



void loop() {
  //uint8_t data[] = "Message 1";
  i!=i;
  
  if (NODE_TYPE == COLLECTOR) {
    Data data = { .id = 1 };
    if (send_message((uint8_t*)&data, sensorArray[i])) {
        Serial.println("Sent data. Waiting for return data.");
        uint8_t len; //recvfromAck should be copying length of payload to len, but doesn't seem to be doing so
        uint8_t from;
        clear_recv_buffer();
        if (router->recvfromAck(recv_buf, &len, &from)) {
            Serial.print("Received message from: ");
            Serial.print(from, DEC);
            Serial.print(" size: ");
            Serial.print(len, DEC);
            Serial.print(" hash: ");
            Serial.println(hash2(recv_buf, sizeof(Data)));
            Serial.print("Message ID: ");
            Serial.println( ((struct Data*)recv_buf)->id, DEC);
        }
    }
    delay(5000);
  } else if (NODE_TYPE == SENSOR) {
      uint8_t len; //recvfromAck should be copying length of payload to len, but doesn't seem to be doing so
      uint8_t from;
      clear_recv_buffer();
      if (router->recvfromAck(recv_buf, &len, &from)) {
          Serial.print("Received message from: ");
          Serial.print(from, DEC);
          Serial.print(" size: ");
          Serial.print(len, DEC);
          Serial.print(" hash: ");
          Serial.println(hash2(recv_buf, sizeof(Data)));
          Serial.print("Message ID: ");
          Serial.println( ((struct Data*)recv_buf)->id, DEC);
          Serial.print("Message bytes: ");
          for (int i=0; i<sizeof(Data); i++) Serial.print(recv_buf[i], DEC);
          Serial.println("");
          if (send_message(recv_buf, from)) {
              Serial.println("Returned data");
          }
      }
  }
    /*
    if (router->sendtoWait(data, sizeof(data), 14) != RH_ROUTER_ERROR_NONE) {
      Serial.println("sendtoWait failed");
    } */
    /*
    if (send_message((uint8_t*)data, 14)) {
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
    */
}
  

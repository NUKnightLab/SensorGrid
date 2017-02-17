#define VERSION 0.11
#include "config.h"
#if BOARD == FeatherM0
    #include "lib/dtostrf.h"
#endif
#include "featherCommon.h"
#include "GPS.h"
#include "LoRa.h"
#include "encryption.h"
//#include <SPI.h>
#include <MemoryFree.h>

/* Modules */
#if WIFI_MODULE == WINC1500
    #include <WiFi101.h>
    #include "modules/wifi/WINC1500.h"
#endif

#if DUST_SENSOR == SHARP_GP2Y1010AU0F
    #include "modules/dust/SHARP_GP2Y1010AU0F.h"
#endif

#include "Adafruit_SI1145.h"
#include "Adafruit_Si7021.h"

#define MAX_MSG_LENGTH 160
typedef struct msgStruct {
  int snd;
  int orig;
  float ver;
  int id;
  float bat;
};
int MSG_ID = 0;
int maxIDs[5] = {0};
char txData[MAX_MSG_LENGTH] = {0};
unsigned long lastTransmit = 0;

Adafruit_Si7021 sensorSi7021TempHumidity = Adafruit_Si7021();
Adafruit_SI1145 sensorSi1145UV = Adafruit_SI1145();
bool sensorSi7021Module = false;
bool sensorSi1145Module = false;


void _receive() {
    //uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    //uint8_t len = sizeof(buf); 
    //uint8_t buf[sizeof(msgStruct)];
    char buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);  
    
    
    if (rf95.recv(buf, &len)) {

        char* msg = (char*)buf;
        struct msgStruct *msgReceived = (struct msgStruct*)msg;
        int senderID = msgReceived->snd;
        int origSenderID = msgReceived->orig;
        float ver = msgReceived->ver;     
        int id = msgReceived->id;
        float bat = msgReceived-> bat;
        Serial.println("RECEIVED (into struct): ");
        Serial.print("    snd: "); Serial.println(senderID);
        Serial.print("    orig: "); Serial.println(origSenderID);
        Serial.print("    ver: "); Serial.println(ver);
        Serial.print("    id: "); Serial.println(id);
        Serial.print("    bat: "); Serial.println(bat);
        
        /*
        int msgID = strtol(msg+24, NULL, 10);
        int senderID = msg[4] - 48;
        // TODO: parse out orig sender
        int origSenderID = strtol(msg+19, NULL, 10);
        if (DEBUG) {
            Serial.println(F("RECEIVED:"));
            Serial.print(F("    Sender: ")); Serial.print(senderID);
            Serial.print(F("; Original Sender: ")); Serial.print(origSenderID);
            Serial.print(F("; MSG ID: ")); Serial.println(msgID);
            Serial.print(F("    Message: ")); Serial.println(msg);
            Serial.print(F("    RSSI: ")); // min recommended RSSI: -91
            Serial.println(rf95.lastRssi(), DEC);
        }
        flashLED(1, HIGH);
        if (  senderID != NODE_ID 
              && origSenderID != NODE_ID 
              && maxIDs[origSenderID] < msgID) {

            // TODO: If end-node (i.e. Wifi) and successful write to API, we don't need to re-transmit
            buf[4] = NODE_ID + 48;
            if (DEBUG) {
                Serial.println(F("RETRANSMITTING:"));
                Serial.print(F("    Message: ")); Serial.println((char*)buf);
            }
            
            rf95.send(buf, sizeof(buf));
            rf95.waitPacketSent();
            flashLED(3, HIGH);
            maxIDs[origSenderID] = msgID;
            if (DEBUG) {
                Serial.println(F("    TRANSMITTED"));
            }           
            
            #if WIFI_MODULE
                if (WIFI_CLIENT.connect(API_SERVER, API_PORT)) {
                    Serial.println(F("API:"));
                    Serial.print(F("    Connected to API server: ")); Serial.print(API_SERVER);
                    Serial.print(F(":")); Serial.println(API_PORT);
                    char sendMsg[500];
                    sprintf(sendMsg, F("GET /?msg=%s HTTP/1.1"), msg);
                    WIFI_CLIENT.println(sendMsg);
                    WIFI_CLIENT.print(F("Host: ")); WIFI_CLIENT.println(API_HOST);
                    WIFI_CLIENT.println(F("Connection: close"));
                    WIFI_CLIENT.println();
                    Serial.print("    "); Serial.println(sendMsg);
                } else {
                  Serial.println(F("Could not connect to API server"));
                }
            #endif
        }
        */
        delete msg;
    }
}

void receive() { 
    if (DEBUG) {
      Serial.println(F("Listening for LoRa signal"));
    } 
    if (rf95.waitAvailableTimeout(10000)) {
        _receive();
    } else {
            Serial.println(F("No message received"));
    }  
}


void append(char *str, char *newStr) {
  strcpy(str+strlen(str), newStr);
}

void appendInt(char *str, int val) {
  itoa(val, str+strlen(str), 10);
}

void appendInt(char *str, int val, int zerofill) {
  // up to 5 char
  char valString[5];
  itoa(val, valString, 10);
  char tmp[zerofill] = {'0'};
  strcpy(tmp+(zerofill-strlen(valString)), valString);
  append(str, tmp);
}

void appendFloat(char *str, float val, int precision) {
   itoa((int)val, str+strlen(str), 10);
   strcpy(str+strlen(str), ".");
   // regular modulo % on casted ints give us bad vals for the GPS data for some reason
   // thus, using fmod
   int fraction = abs((int)(fmod(val, 1)*precision));
   // TODO: zero fill fraction to width of precision
   itoa(fraction, str+strlen(str), 10);
}




void transmit() {
      // TODO: if WiFi node, should write to API instead of transmitting     
      //uint8_t data[] = "sample data"; // should we be using uint8_t instead of char?
      MSG_ID += 1;
      float bat = batteryLevel();
      
      
      Serial.print(F("BAT: ")); Serial.println(bat);

      /*
      memset(&txData[0], 0, MAX_MSG_LENGTH);
      strcpy(txData, "snd=");
      appendInt(txData, NODE_ID);
      append(txData, "&v=");
      append(txData, VERSION);
      append(txData, "&orig=");
      appendInt(txData, NODE_ID);
      append(txData, "&id=");
      appendInt(txData, MSG_ID);
      append(txData, "&bat=");
      appendFloat(txData, bat, 100);
      append(txData, "&dt=20");
      appendInt(txData, GPS.year, 2);
      append(txData, "-");
      appendInt(txData, GPS.month, 2);
      append(txData, "-");
      appendInt(txData, GPS.day, 2);
      append(txData, "T");
      appendInt(txData, GPS.hour, 2);
      append(txData, ":");
      appendInt(txData, GPS.minute, 2);
      append(txData, ":");
      appendInt(txData, GPS.seconds, 2);

      */
      
      struct msgStruct *msgTransmit = malloc(sizeof(struct msgStruct));
      //struct msgStruct *msgTransmit;
      char *msgTx = (char*)msgTransmit;
      msgTransmit->snd = NODE_ID; 
      msgTransmit->orig = NODE_ID;
      //msgTransmit->ver = VERSION;
   
      //uint8_t *msgPtr = (uint8_t*)&msg;
      //uint8_t *msgPtr = (uint8_t*)&msg;
      
      //memcpy(&msgRx, msgPtr, sizeof msgRx);
      
      //struct msgStruct *structuredMessage;
      //structuredMessage = (struct msgStruct*)msgPtr;
      //struct messageStruct *structuredMessage = (struct messageStruct*)msgPtr2;

      /*  
      Serial.print(F("GPS ")); Serial.println(GPS.fix? F("(Fix)") : F("(No Fix)"));
      Serial.print(F("    YEAR: ")); Serial.print(GPS.year, DEC);
      Serial.print(F("; MONTH: ")); Serial.print(GPS.month, DEC);
      Serial.print(F("; DATE: ")); Serial.print(GPS.day, DEC);
      Serial.print(F("; TIME: "));
      Serial.print(GPS.hour); Serial.print(F(":"));
      Serial.print(GPS.minute); Serial.print(F(":"));
      Serial.println(GPS.seconds);
      */

      /*
      if (GPS.fix) {         
          float lat = GPS.latitudeDegrees;
          float lon = GPS.longitudeDegrees;
          Serial.print(F("    LAT: ")); Serial.print(lat, 4);
          Serial.print(F("; LON: ")); Serial.print(lon, 4);
          Serial.print(F("; SATS: ")); Serial.println((int)GPS.satellites);
          append(txData, "&lat=");
          appendFloat(txData, lat, 10000);
          append(txData, "&lon=");
          appendFloat(txData, lon, 10000);
          append(txData, "&sats=");
          appendInt(txData, GPS.satellites);  
      }

      if (sensorSi7021Module) {
          Serial.println(F("TEMP/HUMIDITY:"));   
          float temp = sensorSi7021TempHumidity.readTemperature();
          float humid = sensorSi7021TempHumidity.readHumidity();
          Serial.print(F("    TEMP: ")); Serial.print(temp);
          Serial.print(F("; HUMID: ")); Serial.println(humid);
          append(txData, "&temp=");
          appendFloat(txData, temp, 100);
          append(txData, "&humid=");
          appendFloat(txData, humid, 100);     
      }

      if (sensorSi1145Module) {
          Serial.println(F("Vis/IR/UV:"));
          int vis = sensorSi1145UV.readVisible();
          int ir = sensorSi1145UV.readIR();
          int uv = sensorSi1145UV.readUV();
          Serial.print(F("    VIS: ")); Serial.print(vis);
          Serial.print(F("; IR: ")); Serial.print(ir);
          Serial.print(F("; UV: ")); Serial.println(uv);
          append(txData, "&vis=");
          appendInt(txData, vis);
          append(txData, "&ir=");
          appendInt(txData, ir);
          append(txData, "&uv=");
          appendInt(txData, uv);
      }
      #if DUST_SENSOR
          Serial.println(F("DUST:"));
          float dust = readDustSensor();
          append(txData, "&dust=");
          appendFloat(txData, dust, 100);
      #endif
      */
      //encrypt(data, 128);
      //rf95.send((const uint8_t*)txData, sizeof(txData));
      //rf95.send(msgPtr, sizeof(msg));
      //uint8_t *msgTx = (uint8_t*)&msg;

      Serial.println("transmitting");
      //char *msgTx = (char*)msgTransmit;
      //const uint8_t *msgTx2 = (const uint8_t*)msg;
      
      //Serial.print("Size of msg: "); Serial.println(sizeof(msg));
      //Serial.print("Size of msg thing: "); Serial.print(sizeof(*msg));
      //Serial.print("Size of msgTx: "); Serial.println(sizeof(msgTx));
      //Serial.print("Size of msgStruct: "); Serial.println(sizeof(msgStruct));
      //Serial.print("Size of msgTx2: "); Serial.println(sizeof(msgTx2));
           
      rf95.send(msgTx, sizeof(msgStruct));
      rf95.waitPacketSent();
      Serial.println("transmitted");
      char *msgBuf[sizeof(msgTx)] = {0};
      //uint8_t *msgBuf2[sizeof(msgTx2)] = {0};
      memcpy(msgBuf, msgTx, sizeof(msgStruct));
      //memcpy(msgBuf2, msgTx2, sizeof(msgStruct));
      
      struct msgStruct *msgRx = (struct msgStruct*)msgBuf;
      //struct msgStruct *msgRx2 = (struct msgStruct*)msgBuf2;
      
      Serial.print("STRUCT snd: "); Serial.println(msgRx->snd);
      Serial.print("STRUCT orig: "); Serial.println(msgRx->orig);
      //Serial.print("STRUCT ver: "); Serial.println(msgRx->ver);
      
      Serial.print(F("TRANSMIT:\n    "));
      //Serial.println(txData);
      free(msgTransmit);
      //delete(msgTx);
      //delete msgTransmit;
      flashLED(3, HIGH);
}

void setup() {
    if (DEBUG) {
        while (!Serial); // only do this if connected to USB
    }
    Serial.begin(9600);
    //Serial.begin(115200);
    //Serial.begin(19200);
    if (DEBUG) {
        Serial.println(F("Serial ready"));
    }

    flashLED(2, HIGH);
    #if DUST_SENSOR
        setupDustSensor();
    #endif

    Serial.print(F("Max RH_RF95 Message length: "));
    Serial.println(RH_RF95_MAX_MESSAGE_LEN);
    Serial.print(F("Battery pin set to: "));
    if (VBATPIN == A7) {
        Serial.println("A7");
    } else {
        Serial.println("A9");
    }
    if (CHARGE_ONLY) {
      return;
    }
    pinMode(LED, OUTPUT);
    pinMode(RFM95_RST, OUTPUT);
    digitalWrite(RFM95_RST, HIGH);

    setupGPS();
    setupRadio();

    #if WIFI_MODULE
        setupWiFi();
    #endif

    if (sensorSi7021TempHumidity.begin()) {
        Serial.println(F("Found Si7021"));
        sensorSi7021Module = true;
    } else {
        Serial.println(F("Si7021 Not Found"));
    }
    
    if (sensorSi1145UV.begin()) {
        Serial.println(F("Found Si1145"));
        sensorSi1145Module = true;
    } else {
        Serial.println(F("Si1145 Not Found"));
    }

    Serial.println("OK!");
}

void loop() {


    Serial.print(F("freeMemory: "));
    Serial.print(freeMemory());
    Serial.print(F("; freeRam: "));
    Serial.println(freeRam());


    if (CHARGE_ONLY) {
      Serial.print(F("BAT: ")); Serial.println(batteryLevel());
      delay(10000);
      return;
    }

    if (RECEIVE) {
        receive();
    }

    if ( TRANSMIT && (millis() - lastTransmit) > 1000 * 10) {
        Serial.println(F("--------------------------------------------------------"));
        transmit();
        lastTransmit = millis();
        Serial.println(F("********************************************************"));
    }
}

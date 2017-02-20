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
  uint8_t hour, minute, seconds, year, month, day;
  bool fix;
  float lat, lon;
  int sats;
};
int MSG_ID = 0;
int maxIDs[5] = {0};
//char txData[MAX_MSG_LENGTH] = {0};
unsigned long lastTransmit = 0;

Adafruit_Si7021 sensorSi7021TempHumidity = Adafruit_Si7021();
Adafruit_SI1145 sensorSi1145UV = Adafruit_SI1145();
bool sensorSi7021Module = false;
bool sensorSi1145Module = false;

//struct msgStruct *msgTransmit = malloc(sizeof(struct msgStruct));
//struct msgStruct msgTransmit[sizeof(msgStruct)] = {0};
//char *msgTx = (char*)msgTransmit;

//char receiveBuffer[RH_RF95_MAX_MESSAGE_LEN];
char receiveBuffer[sizeof(msgStruct)];
uint8_t receiveBufferLen = sizeof(receiveBuffer);
char* msg = (char*)receiveBuffer;
struct msgStruct *rx = (struct msgStruct*)msg;
struct msgStruct *msgTransmit = rx;
char *msgTx = (char*)msgTransmit;

void _receive() {
    //uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    //uint8_t len = sizeof(buf);
    //uint8_t buf[sizeof(msgStruct)];
    //char buf[RH_RF95_MAX_MESSAGE_LEN];
    //uint8_t len = sizeof(buf);
    memset(&receiveBuffer, 0, receiveBufferLen);
    if (rf95.recv(receiveBuffer, &receiveBufferLen)) {
        //char* msg = (char*)buf;
        //struct msgStruct *rx = (struct msgStruct*)msg;
        int senderID = rx->snd;
        int origSenderID = rx->orig;
        float ver = rx->ver;
        int msgID = rx->id;
        float bat = rx-> bat;
        Serial.println(F("RECEIVED (into struct): "));
        Serial.print(F("    RSSI: ")); // min recommended RSSI: -91
        Serial.println(rf95.lastRssi(), DEC);
        Serial.print(F("    snd: ")); Serial.println(senderID);
        Serial.print(F("    orig: ")); Serial.println(origSenderID);
        Serial.print(F("    ver: ")); Serial.println(ver);
        Serial.print(F("    id: ")); Serial.println(msgID);
        Serial.print(F("    bat: ")); Serial.println(bat);
        Serial.print(F("    DATETIME: "));
        Serial.print(rx->year); Serial.print(F("-"));
        Serial.print(rx->month); Serial.print(F("-"));
        Serial.print(rx->day); Serial.print(F("T"));
        Serial.print(rx->hour); Serial.print(F(":"));
        Serial.print(rx->minute); Serial.print(F(":"));
        Serial.println(rx->seconds);
        Serial.print(F("    fix: ")); Serial.print(rx->fix);
        Serial.print(F("; lat: ")); Serial.print(rx->lat);
        Serial.print(F("; lon: ")); Serial.print(rx->lon);
        Serial.print(F("; sats: ")); Serial.println(rx->sats);
        flashLED(1, HIGH);

        /* TODO: This won't properly handle a node restart b/c the message IDs
         *  start back over from 1
         */
        if (  senderID != NODE_ID
              && maxIDs[origSenderID] < msgID) {

            if (origSenderID == NODE_ID) {
                Serial.print(F("Received own message: ")); Serial.println(msgID);
            } else {
                // TODO: If end-node (i.e. Wifi) and successful write to API, we don't need to re-transmit
                //buf[4] = NODE_ID + 48;
                rx->snd = NODE_ID;
                Serial.println(F("RETRANSMITTING ..."));
                //rf95.send(buf, sizeof(buf));
                rf95.send(msg, sizeof(msgStruct));
                rf95.waitPacketSent();
                flashLED(3, HIGH);
                maxIDs[origSenderID] = msgID;
                Serial.println(F("    ...RETRANSMITTED"));
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
                    Serial.print(F("    ")); Serial.println(sendMsg);
                } else {
                  Serial.println(F("Could not connect to API server"));
                }
            #endif
        }
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


void constructQueryString() {
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
}

void transmit() {
      // TODO: if WiFi node, should write to API instead of transmitting
      //uint8_t data[] = "sample data"; // should we be using uint8_t instead of char?
      MSG_ID += 1;
      memset(&receiveBuffer, 0, receiveBufferLen);
      float bat = batteryLevel();
      //Serial.print(F("malloc tx msg: ")); Serial.println(sizeof(struct msgStruct));
      //memset(msgTransmit, 0, sizeof(msgStruct));
      //struct msgStruct *msgTransmit = malloc(sizeof(struct msgStruct));
      //struct msgStruct *msgTransmit;
      //Serial.println("cast to char*");
      //char *msgTx = (char*)msgTransmit;
      msgTransmit->snd = NODE_ID;
      msgTransmit->orig = NODE_ID;
      msgTransmit->ver = VERSION;
      msgTransmit->id = MSG_ID;
      msgTransmit->bat = bat;
      msgTransmit->hour = GPS.hour;
      msgTransmit->minute = GPS.minute;
      msgTransmit->seconds = GPS.seconds;
      msgTransmit->year = GPS.year;
      msgTransmit->month = GPS.month;
      msgTransmit->day = GPS.day;
      msgTransmit->fix = GPS.fix;
      msgTransmit->lat = GPS.latitudeDegrees;
      msgTransmit->lon = GPS.longitudeDegrees;
      msgTransmit->sats = GPS.satellites;
      Serial.println(F("Data assigned"));

      /*
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
      // old char array/query-string based transmit
      //rf95.send((const uint8_t*)txData, sizeof(txData));

      rf95.send(msgTx, sizeof(msgStruct));
      rf95.waitPacketSent();
      Serial.println(F("Transmitted"));

      /*
      char *msgBuf[sizeof(msgStruct)] = {0};
      memcpy(msgBuf, msgTx, sizeof(msgStruct));

      struct msgStruct *msgRx = (struct msgStruct*)msgBuf;
      Serial.println("TRANSMIT (Test struct):");
      Serial.print("    snd: "); Serial.println(msgRx->snd);
      Serial.print("    orig: "); Serial.println(msgRx->orig);
      Serial.print("    ver: "); Serial.println(msgRx->ver);
      Serial.print("    id: "); Serial.println(msgRx->id);
      Serial.print("    bat: "); Serial.println(msgRx->bat);
      Serial.print("    DATETIME: ");
      Serial.print(msgRx->year); Serial.print("-");
      Serial.print(msgRx->month); Serial.print("-");
      Serial.print(msgRx->day); Serial.print("T");
      Serial.print(msgRx->hour); Serial.print(":");
      Serial.print(msgRx->minute); Serial.print(":");
      Serial.println(msgRx->seconds);
      Serial.print("    fix: "); Serial.print(msgRx->fix);
      Serial.print("; lat: "); Serial.print(msgRx->lat);
      Serial.print("; lon: "); Serial.print(msgRx->lon);
      Serial.print("; sats: "); Serial.println(msgRx->sats);
      */
      //free(msgTransmit);
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
        Serial.println(F("A7"));
    } else {
        Serial.println(F("A9"));
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

    //msgTransmit->snd = NODE_ID;
    //msgTransmit->orig = NODE_ID;
    //msgTransmit->ver = VERSION;

    Serial.println(F("OK!"));
}

void printRam() {
    Serial.print(F("freeMemory: "));
    Serial.print(freeMemory());
    Serial.print(F("; freeRam: "));
    Serial.println(freeRam());
}

void loop() {

    if (CHARGE_ONLY) {
      Serial.print(F("BAT: ")); Serial.println(batteryLevel());
      delay(10000);
      return;
    }

    if (RECEIVE) {
        Serial.println(F("Ready to receive ..."));
        printRam();
        Serial.println(F("--------------------------------------------------------"));
        receive();
        Serial.println(F("********************************************************"));
    }

    if ( TRANSMIT && (millis() - lastTransmit) > 1000 * 10) {
        Serial.println(F("Ready to transmit ..."));
        printRam();
        Serial.println(F("--------------------------------------------------------"));
        transmit();
        lastTransmit = millis();
        Serial.println(F("********************************************************"));
    }
}

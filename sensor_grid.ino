#define VERSION 0.11
#include "config.h"
#if BOARD == FeatherM0
    #include "lib/dtostrf.h"
#endif
#include "featherCommon.h"
#include "GPS.h"
#include "LoRa.h"
#include "encryption.h"
#include <SPI.h>

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

char codeVersion[5];
int MSG_ID = 0;
int maxIDs[5] = {0};

Adafruit_Si7021 sensorSi7021TempHumidity = Adafruit_Si7021();
Adafruit_SI1145 sensorSi1145UV = Adafruit_SI1145();
bool sensorSi7021Module = false;
bool sensorSi1145Module = false;

void setup() {
    dtostrf(VERSION, 4, 2, codeVersion);
    if (DEBUG) {
        while (!Serial); // only do this if connected to USB
    }
    Serial.begin(9600);  //Serial.begin(115200);
    if (DEBUG) {
        Serial.println("Serial ready");
    }
    flashLED(2, HIGH);
    #if DUST_SENSOR
        setupDustSensor();
    #endif

    Serial.print("Battery pin set to: ");
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
        Serial.println("Found Si7021");
        sensorSi7021Module = true;
    } else {
        Serial.println("Si7021 Not Found");
    }

    /* TODO: Is UV sensor getting fried?
    if (sensorSi1145UV.begin()) {
        Serial.println("Found Si1145");
        sensorSi1145Module = true;
    } else {
        Serial.println("Si1145 Not Found");
    }
    */
    
    Serial.println("OK!");
}

void _receive() {
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    if (rf95.recv(buf, &len)) {
        char* msg = (char*)buf;
        int msgID = strtol(msg+11, NULL, 10);
        int senderID = msg[4] - 48;
        int origSenderID = strtol(msg+9, NULL, 10);

        /* log msg */
        if (DEBUG) {
            Serial.println("RECEIVED:");
            Serial.print("    Sender: "); Serial.print(senderID);
            Serial.print("; Original Sender: "); Serial.print(origSenderID);
            Serial.print("; MSG ID: "); Serial.println(msgID);
            Serial.print("    Message: "); Serial.println(msg);
            Serial.print("    RSSI: "); // min recommended RSSI: -91
            Serial.println(rf95.lastRssi(), DEC);
        }
        flashLED(1, HIGH);

        if (  senderID != NODE_ID 
              && origSenderID != NODE_ID 
              && maxIDs[origSenderID] < msgID) {

            // TODO: If end-node (i.e. Wifi) and successful write to API, we don't need to re-transmit
            buf[4] = NODE_ID + 48;
            if (DEBUG) {
                Serial.println("RETRANSMITTING:");
                Serial.print("    "); Serial.println((char*)buf);
            }
            rf95.send(buf, sizeof(buf));
            rf95.waitPacketSent();
            flashLED(3, HIGH);
            maxIDs[origSenderID] = msgID;
            if (DEBUG) {
                Serial.println("    TRANSMITTED");
            }

            #if WIFI_MODULE
                if (WIFI_CLIENT.connect(API_SERVER, API_PORT)) {
                    Serial.println("API:");
                    Serial.print("    Connected to API server: "); Serial.print(API_SERVER);
                    Serial.print(":"); Serial.println(API_PORT);
                    char sendMsg[500];
                    sprintf(sendMsg, "GET /?msg=%s HTTP/1.1", msg);
                    WIFI_CLIENT.println(sendMsg);
                    WIFI_CLIENT.print("Host: "); WIFI_CLIENT.println(API_HOST);
                    WIFI_CLIENT.println("Connection: close");
                    WIFI_CLIENT.println();
                    Serial.print("    "); Serial.println(sendMsg);
                } else {
                  Serial.println("Could not connect to API server");
                }
            #endif
        }
    }
}

void receive() {
    if (DEBUG) {
      Serial.println("Listening for LoRa signal");
    }
    if (rf95.waitAvailableTimeout(10000)) {
        _receive();
    } else {
            Serial.println("No message received");
    }
}

void transmit() {
      // TODO: if WiFi node, should write to API instead of transmitting     
      //uint8_t data[] = "sample data"; // should we be using uint8_t instead of char?
      char data[100];
      char bat[5];
      dtostrf(batteryLevel(), 4, 2, bat);
      MSG_ID += 1;
      readGPS();
      if (GPS.fix) {
          if (DEBUG) {
              Serial.println("GPS Fix");
          }
          char lat[8];
          char lon[8];
          dtostrf(GPS.latitude, 7, 3, lat);
          dtostrf(GPS.longitude, 7, 3, lon);
          sprintf(data,
            "snd=%d&id=%d.%04d&v=%s&bat=%s&dt=20%d-%d-%dT%d:%d:%d.%d&loc=%s,%s",
            NODE_ID, NODE_ID, MSG_ID, codeVersion, bat, GPS.year, GPS.month, GPS.day,
            GPS.hour, GPS.minute, GPS.seconds, GPS.milliseconds,
            lat, lon);
      } else {
          if (DEBUG) {
              Serial.println("No GPS Fix");
          }
          sprintf(data,
            "snd=%d&id=%d.%04d&v=%s&bat=%s&dt=20%d-%d-%dT%d:%d:%d.%d",
            NODE_ID, NODE_ID, MSG_ID, codeVersion, bat, GPS.year, GPS.month, GPS.day,
            GPS.hour, GPS.minute, GPS.seconds, GPS.milliseconds);
      }
      if (sensorSi7021Module) {
          if (DEBUG) {
              Serial.println("Checking for temp/humid module reading");
          }     
          char temp[5];
          char humid[5];
          dtostrf(sensorSi7021TempHumidity.readTemperature(), 4, 2, temp);
          dtostrf(sensorSi7021TempHumidity.readHumidity(), 4, 2, humid);
          sprintf(data, "%s&temp=%s&humid=%s", data, temp, humid);
      }
      if (sensorSi1145Module) {
          if (DEBUG) {
              Serial.println("Checking for Vis/IR/UV light module reading");
          }
          //char uv[5];
          int vis = sensorSi1145UV.readVisible();
          int ir = sensorSi1145UV.readIR();
          //dtostrf(sensorSi1145UV.readUV()/100.0, 4, 2, uv);
          sprintf(data, "%s&vis=%d&ir=%d", data, vis, ir);
      }
      #if DUST_SENSOR
          if (DEBUG) {
              Serial.println("Checking for dust module reading");
          }
          char dustDensity[5];
          dtostrf(readDustSensor(), 4, 2, dustDensity);
          sprintf(data, "%s&dust=%s", data, dustDensity);
      #endif
      //encrypt(data, 128);
      rf95.send((const uint8_t*)data, sizeof(data));
      rf95.waitPacketSent();
      Serial.print("TRANSMIT:\n    ");
      Serial.println(data);
      flashLED(3, HIGH);
}

unsigned long lastTransmit = 0;

void loop() {
    
    if (CHARGE_ONLY) {
      Serial.print("BAT: "); Serial.println(batteryLevel());
      delay(10000);
      return;
    }

    receive();
    
    if ( (millis() - lastTransmit) > 1000 * 10) {
        Serial.println("--------------------------------------------------------");
        Serial.print("ID: "); Serial.print(NODE_ID);
        Serial.print("; BAT: "); Serial.println(batteryLevel());
        //readGPS();
        Serial.println("SENSORS:");
        if (sensorSi7021Module) {
            Serial.print("    Temperature: ");
            Serial.print(sensorSi7021TempHumidity.readTemperature(), 2);
            Serial.print("; Humidity: ");
            Serial.println(sensorSi7021TempHumidity.readHumidity(), 2);
        }
        if (sensorSi1145Module) {
            Serial.print("    Vis: "); Serial.print(sensorSi1145UV.readVisible());
            Serial.print("; IR: "); Serial.print(sensorSi1145UV.readIR());
            float UVindex = sensorSi1145UV.readUV();
            UVindex /= 100.0;
            Serial.print("; UV: ");  Serial.println(UVindex);
        }
        #if DUST_SENSOR
            float dustDensity = readDustSensor();
            Serial.print("    Dust: "); Serial.println(dustDensity);
        #endif
        transmit();
        lastTransmit = millis();
        Serial.println("********************************************************");
    }

    //delay(100);
}

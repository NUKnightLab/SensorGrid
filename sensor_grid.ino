#define VERSION_A 0
#define VERSION_B 11
#include "errors.h"
#include "config.h"
#if BOARD == FeatherM0
    #include "lib/dtostrf.h"
#endif
#include "featherCommon.h"
#include "GPS.h"
#include "LoRa.h"
#include "encryption.h"
//#include <SPI.h>

/* Modules */
#if WIFI_MODULE == WINC1500
    //#include <WiFi101.h>
    #include "modules/wifi/WINC1500.h"
#endif

#if DUST_SENSOR == SHARP_GP2Y1010AU0F
    #include "modules/dust/SHARP_GP2Y1010AU0F.h"
#endif

#include "Adafruit_SI1145.h"
#include "Adafruit_Si7021.h"

enum DATA_TYPES {
    VISIBLE_LIGHT,
    IR_LIGHT,
    UV_LIGHT,
    TEMPERATURE_100,
    HUMIDITY_100,
    DUST_100
};


typedef struct Message {
    uint8_t snd;
    uint8_t orig;
    uint8_t ver_a;
    uint8_t ver_b;
    uint32_t id;
    int16_t bat_100;
    uint8_t hour, minute, seconds, year, month, day;
    bool fix;
    int16_t lat_100, lon_100;
    uint8_t sats;
    int32_t data[6];
};

int MSG_ID = 0;
int maxIDs[5] = {0};
unsigned long lastTransmit = 0;

Adafruit_Si7021 sensorSi7021TempHumidity = Adafruit_Si7021();
Adafruit_SI1145 sensorSi1145UV = Adafruit_SI1145();
bool sensorSi7021Module = false;
bool sensorSi1145Module = false;

//char msgBytes[sizeof(Message)];
uint8_t msgBytes[sizeof(Message)];
uint8_t msgLen = sizeof(msgBytes);
struct Message *msg = (struct Message*)msgBytes;

void clearMessage() {
    memset(&msgBytes, 0, msgLen);
}

void sendCurrent() {
    rf95.send(msgBytes, sizeof(Message));
    rf95.waitPacketSent();
    flashLED(3, HIGH);
}

void _receive() {
    clearMessage();
    if (rf95.recv(msgBytes, &msgLen)) {
        Serial.print(F(" ..RX (ver: ")); Serial.print(msg->ver_a);
        Serial.print(F(".")); Serial.print(msg->ver_b); Serial.println(")");
        Serial.print(F("    RSSI: ")); // min recommended RSSI: -91
        Serial.print(rf95.lastRssi(), DEC);
        Serial.print(F("; SND: ")); Serial.print(msg->snd);
        Serial.print(F("; ORIG: ")); Serial.print(msg->orig);
        Serial.print(F("; ID: ")); Serial.println(msg->id);
        Serial.print(F("    BAT: ")); Serial.print(msg->bat_100/100);
        Serial.print(F("; DT: "));
        Serial.print(msg->year); Serial.print(F("-"));
        Serial.print(msg->month); Serial.print(F("-"));
        Serial.print(msg->day); Serial.print(F("T"));
        Serial.print(msg->hour); Serial.print(F(":"));
        Serial.print(msg->minute); Serial.print(F(":"));
        Serial.println(msg->seconds);
        Serial.print(F("    fix: ")); Serial.print(msg->fix);
        Serial.print(F("; lat: ")); Serial.print(msg->lat_100/100);
        Serial.print(F("; lon: ")); Serial.print(msg->lon_100/100);
        Serial.print(F("; sats: ")); Serial.println(msg->sats);
        Serial.print(F("    Temp: ")); Serial.print(msg->data[TEMPERATURE_100]);
        Serial.print(F("; Humid: ")); Serial.println(msg->data[HUMIDITY_100]);
        Serial.print(F("    Dust: ")); Serial.println(msg->data[DUST_100]);
        Serial.print(F("    Vis: ")); Serial.print(msg->data[VISIBLE_LIGHT]);
        Serial.print(F("; IR: ")); Serial.print(msg->data[IR_LIGHT]);
        Serial.print(F("; UV: ")); Serial.println(msg->data[UV_LIGHT]);
        flashLED(1, HIGH);

        if ( msg->orig == NODE_ID ) {
            Serial.print(F("NO-OP. OWN MSG: ")); Serial.println(msg->id);
        } else {
            if (msg->id <= maxIDs[msg->orig]) {
                Serial.print(F("Ignore old Msg: "));
                Serial.print(msg->orig); Serial.print("."); Serial.print(msg->id);
                Serial.print(F("(Current Max: ")); Serial.print(maxIDs[msg->orig]);
                Serial.println(F(")"));
                if (maxIDs[msg->orig] - msg->id > 5) {
                    /* Crude handling of node reset will drop some packets but eventually
                     * start again. TODO: A better approach would be for each node to store
                     * its Max ID in EEPROM and get rid of this reset - but EEPROM on
                     * M0 Feather boards is complex (if available at all?). See:
                     * https://forums.adafruit.com/viewtopic.php?f=22&t=88272
                     */ 
                    Serial.println(F("Reset Max ID. Node: "));
                    Serial.println(msg->orig);
                    maxIDs[msg->orig] = 0;
                }
            } else {
                /* TODO: If end-node (i.e. Wifi) and successful write to API, 
                 *  we don't need to re-transmit
                 */
                msg->snd = NODE_ID;
                delay(1000); // needed for sending radio to receive the bounce
                Serial.print(F("RETRANSMITTING ..."));
                Serial.print(F("  snd: ")); Serial.print(msg->snd);
                Serial.print(F("; orig: ")); Serial.print(msg->orig);
                sendCurrent();            
                maxIDs[msg->orig] = msg->id;
                Serial.println(F("  ...RETRANSMITTED"));
            }

            #if WIFI_MODULE
                if (WIFI_CLIENT.connect(API_SERVER, API_PORT)) {
                    Serial.println(F("API:"));
                    Serial.print(F("    CON: "));
                    Serial.print(API_SERVER);
                    Serial.print(F(":")); Serial.println(API_PORT);
                    char sendMsg[500];
                    //sprintf(sendMsg, F("GET /?msg=%s HTTP/1.1"), msg);
                    //WIFI_CLIENT.println(sendMsg);
                    WIFI_CLIENT.print(F("Host: ")); WIFI_CLIENT.println(API_HOST);
                    WIFI_CLIENT.println(F("CLOSE"));
                    WIFI_CLIENT.println();
                    //Serial.print(F("    ")); Serial.println(sendMsg);
                } else {
                  Serial.println(F("FAIL: API CON"));
                }
            #endif
        }
    }
}

void receive() {
    // randomized receive cycle to avoid loop sync across nodes
    int delta = 5000 + rand() % 5000;
    Serial.print(F("NODE ")); Serial.print(NODE_ID);
    Serial.print(F(" LISTEN: ")); Serial.print(delta);
    Serial.print(F("ms"));
    if (rf95.waitAvailableTimeout(delta)) {
        _receive();
    } else {
        Serial.println(F("  ..NO MSG REC"));
    }
}

void append(char *str, char *newStr) {
    //strcpy(str+strlen(str), newStr);
}

void appendInt(char *str, int val) {
    //itoa(val, str+strlen(str), 10);
}

void appendInt(char *str, int val, int zerofill) {
  // up to 5 char
  //char valString[5];
  //itoa(val, valString, 10);
  //char tmp[zerofill] = {'0'};
  //strcpy(tmp+(zerofill-strlen(valString)), valString);
  //append(str, tmp);
}

void appendFloat(char *str, float val, int precision) {
   /*
   itoa((int)val, str+strlen(str), 10);
   strcpy(str+strlen(str), ".");
   // regular modulo % on casted ints give us bad vals for the GPS data for some reason
   // thus, using fmod
   int fraction = abs((int)(fmod(val, 1)*precision));
   // TODO: zero fill fraction to width of precision
   itoa(fraction, str+strlen(str), 10);
   */
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
      MSG_ID++;
      clearMessage();
      msg->snd = NODE_ID;
      msg->orig = NODE_ID;
      msg->ver_a = VERSION_A;
      msg->ver_b = VERSION_B;
      msg->id = MSG_ID;
      msg->bat_100 = (int16_t)(batteryLevel() * 100);
      msg->hour = GPS.hour;
      msg->minute = GPS.minute;
      msg->seconds = GPS.seconds;
      msg->year = GPS.year;
      msg->month = GPS.month;
      msg->day = GPS.day;
      msg->fix = GPS.fix;
      msg->lat_100 = (int16_t)(GPS.latitudeDegrees * 100);
      msg->lon_100 = (int16_t)(GPS.longitudeDegrees * 100);
      msg->sats = GPS.satellites;


      if (sensorSi7021Module) {
          Serial.println(F("TEMP/HUMIDITY:"));
          msg->data[TEMPERATURE_100] = (int32_t)(sensorSi7021TempHumidity.readTemperature()*100);
          msg->data[HUMIDITY_100] = (int32_t)(sensorSi7021TempHumidity.readHumidity()*100);
          Serial.print(F("    TEMP: ")); Serial.print(msg->data[TEMPERATURE_100]);
          Serial.print(F("; HUMID: ")); Serial.println(msg->data[HUMIDITY_100]);
          //append(txData, "&temp=");
          //appendFloat(txData, temp, 100);
          //append(txData, "&humid=");
          //appendFloat(txData, humid, 100);
      }

      if (sensorSi1145Module) {
          Serial.println(F("Vis/IR/UV:"));
          msg->data[VISIBLE_LIGHT] = (int32_t)sensorSi1145UV.readVisible();
          msg->data[IR_LIGHT] = (int32_t)sensorSi1145UV.readIR();
          msg->data[UV_LIGHT] = (int32_t)sensorSi1145UV.readUV();
          Serial.print(F("    VIS: ")); Serial.print(msg->data[VISIBLE_LIGHT]);
          Serial.print(F("; IR: ")); Serial.print(msg->data[IR_LIGHT]);
          Serial.print(F("; UV: ")); Serial.println(msg->data[UV_LIGHT]);
          //append(txData, "&vis=");
          //appendInt(txData, vis);
          //append(txData, "&ir=");
          //appendInt(txData, ir);
          //append(txData, "&uv=");
          //appendInt(txData, uv);
      }

      #if DUST_SENSOR
          msg->data[DUST_100] = (int32_t)(readDustSensor()*100);
          Serial.print(F("DUST: ")); Serial.println(msg->data[DUST_100]);
          //append(txData, "&dust=");
          //appendFloat(txData, dust, 100);
      #endif
      sendCurrent();
      Serial.println(F("!TX"));

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
    if (DEBUG) {
        Serial.println(F("SRL RDY"));
    }

    flashLED(2, HIGH);
    #if DUST_SENSOR
        setupDustSensor();
    #endif
    
    Serial.print(F("BAT: "));
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

    Serial.print(F("Si7021 "));
    if (sensorSi7021TempHumidity.begin()) {
        Serial.println(F("Found"));
        sensorSi7021Module = true;
    } else {
        Serial.println(F("Not Found"));
    }

    Serial.print(F("Si1145 "));
    if (sensorSi1145UV.begin()) {
        Serial.println(F("Found"));
        sensorSi1145Module = true;
    } else {
        Serial.println(F("Not Found"));
    }

    if (sizeof(Message) > RH_RF95_MAX_MESSAGE_LEN) {
        fail(MESSAGE_STRUCT_TOO_LARGE);
    }
    Serial.println(F("OK!"));
}

void loop() {

    if (CHARGE_ONLY) {
      Serial.print(F("BAT: ")); Serial.println(batteryLevel());
      delay(10000);
      return;
    }

    if (RECEIVE) {
        printRam();
        Serial.println(F("---"));
        receive();
        Serial.println(F("***"));
    }

    if ( TRANSMIT && (millis() - lastTransmit) > 1000 * 10) {
        printRam();
        Serial.println(F("---"));
        transmit();
        lastTransmit = millis();
        Serial.println(F("***"));
    }
}

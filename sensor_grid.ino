#include "sensor_grid.h"

/* Modules */
//#if WIFI_MODULE == WINC1500
//    #include "modules/wifi/WINC1500.h"
//#endif

#if DUST_SENSOR == SHARP_GP2Y1010AU0F
    #include "modules/dust/SHARP_GP2Y1010AU0F.h"
#endif

#include "Adafruit_SI1145.h"
#include "Adafruit_Si7021.h"

uint32_t MSG_ID = 0;
int maxIDs[5] = {0};
unsigned long lastTransmit = 0;

Adafruit_Si7021 sensorSi7021TempHumidity = Adafruit_Si7021();
Adafruit_SI1145 sensorSi1145UV = Adafruit_SI1145();
bool sensorSi7021Module = false;
bool sensorSi1145Module = false;


uint8_t msgBytes[sizeof(Message)];
uint8_t msgLen = sizeof(msgBytes);
struct Message *msg = (struct Message*)msgBytes;

void _receive() {
    clearMessage();
    if (rf95.recv(msgBytes, &msgLen)) {
        Serial.print(F(" ..RX (ver: ")); Serial.print(msg->ver_100);
        Serial.print(F(")"));
        Serial.print(F("    RSSI: ")); // min recommended RSSI: -91
        Serial.println(rf95.lastRssi(), DEC);
        printMessageData();

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
                    // Crude handling of node reset will drop some packets but eventually
                    // start again. TODO: A better approach would be for each node to store
                    // its Max ID in EEPROM and get rid of this reset - but EEPROM on
                    // M0 Feather boards is complex (if available at all?). See:
                    // https://forums.adafruit.com/viewtopic.php?f=22&t=88272
                    //
                    Serial.println(F("Reset Max ID. Node: "));
                    Serial.println(msg->orig);
                    maxIDs[msg->orig] = 0;
                }
            } else {
                msg->snd = NODE_ID;
                if (!postToAPI()) {
                    delay(1000); // needed for sending radio to receive the bounce
                    Serial.print(F("RETRANSMITTING ..."));
                    Serial.print(F("  snd: ")); Serial.print(msg->snd);
                    Serial.print(F("; orig: ")); Serial.print(msg->orig);
                    sendCurrentMessage();
                    maxIDs[msg->orig] = msg->id;
                    Serial.println(F("  ...RETRANSMITTED"));
                }
            }
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

void transmit() {
      MSG_ID++;
      clearMessage();
      msg->ver_100 = (uint16_t)(roundf(VERSION * 100));
      msg->net = NETWORK_ID;
      msg->snd = NODE_ID;
      msg->orig = NODE_ID;
      msg->id = MSG_ID;
      msg->bat_100 = (int16_t)(roundf(batteryLevel() * 100));
      msg->hour = GPS.hour;
      msg->minute = GPS.minute;
      msg->seconds = GPS.seconds;
      msg->year = GPS.year;
      msg->month = GPS.month;
      msg->day = GPS.day;
      msg->fix = GPS.fix;
      msg->lat_1000 = (int32_t)(roundf(GPS.latitudeDegrees * 1000));
      msg->lon_1000 = (int32_t)(roundf(GPS.longitudeDegrees * 1000));
      msg->sats = GPS.satellites;

      printMessageData();

      if (sensorSi7021Module) {
          Serial.println(F("TEMP/HUMIDITY:"));
          msg->data[TEMPERATURE_100] = (int32_t)(sensorSi7021TempHumidity.readTemperature()*100);
          msg->data[HUMIDITY_100] = (int32_t)(sensorSi7021TempHumidity.readHumidity()*100);
          Serial.print(F("    TEMP: ")); Serial.print(msg->data[TEMPERATURE_100]);
          Serial.print(F("; HUMID: ")); Serial.println(msg->data[HUMIDITY_100]);
      }

      if (sensorSi1145Module) {
          Serial.println(F("Vis/IR/UV:"));
          msg->data[VISIBLE_LIGHT] = (int32_t)sensorSi1145UV.readVisible();
          msg->data[IR_LIGHT] = (int32_t)sensorSi1145UV.readIR();
          msg->data[UV_LIGHT] = (int32_t)sensorSi1145UV.readUV();
          Serial.print(F("    VIS: ")); Serial.print(msg->data[VISIBLE_LIGHT]);
          Serial.print(F("; IR: ")); Serial.print(msg->data[IR_LIGHT]);
          Serial.print(F("; UV: ")); Serial.println(msg->data[UV_LIGHT]);
      }

      #if DUST_SENSOR
          msg->data[DUST_100] = (int32_t)(readDustSensor()*100);
          Serial.print(F("DUST: ")); Serial.println((float)msg->data[DUST_100]/100);
      #endif

      if (!postToAPI()) {
          sendCurrentMessage();
          Serial.println(F("!TX"));
      }

      flashLED(3, HIGH);
}

void setup() {

    #ifdef DEBUG
        while (!Serial); // only do this if connected to USB
    #endif
    Serial.begin(9600);
    Serial.println(F("SRL RDY"));

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

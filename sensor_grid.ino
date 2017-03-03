#include "sensor_grid.h"

/* Modules */

unsigned long lastTransmit = 0;

Adafruit_Si7021 sensorSi7021TempHumidity = Adafruit_Si7021();
Adafruit_SI1145 sensorSi1145UV = Adafruit_SI1145();
bool sensorSi7021Module = false;
bool sensorSi1145Module = false;

//uint8_t msgBytes[sizeof(Message)];
//uint8_t msgLen = sizeof(msgBytes);
//struct Message *msg = (struct Message*)msgBytes;

//union {
//  struct Message newMsgStruct;
//  uint8_t newMsgBytes[sizeof(Message)];
//} newMsg;

//struct Message newMsg;
bool WiFiPresent = false;

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
    #if defined(ARDUINO_ARCH_SAMD)
      WiFiPresent = setupWiFi();
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
        Serial.println("RX");
        printRam();
        Serial.println(F("---"));
        receive();
        Serial.println(F("***"));
    }

    if ( TRANSMIT && (millis() - lastTransmit) > 1000 * 10) {
        Serial.println("TX");
        printRam();
        Serial.println(F("---"));
        transmit();
        lastTransmit = millis();
        Serial.println(F("***"));
    }
}

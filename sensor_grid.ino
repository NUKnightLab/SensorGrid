#include "sensor_grid.h"
#include "display.h"

uint32_t NETWORK_ID;
uint32_t NODE_ID;
char* LOGFILE;

/* Modules */

unsigned long lastTransmit = 0;

Adafruit_Si7021 sensorSi7021TempHumidity = Adafruit_Si7021();
Adafruit_SI1145 sensorSi1145UV = Adafruit_SI1145();
bool sensorSi7021Module = false;
bool sensorSi1145Module = false;

#if defined(ARDUINO_ARCH_SAMD)
Adafruit_SSD1306 display = Adafruit_SSD1306();
#endif

bool WiFiPresent = false;

void setup() {

    #ifdef DEBUG
        while (!Serial); // only do this if connected to USB
    #endif
    Serial.begin(9600);
    Serial.println(F("SRL RDY"));
    flashLED(2, HIGH);
    
    #if defined(__AVR_ATmega32U4__)
        #include "config.h"
        NETWORK_ID = CONFIG__NETWORK_ID;
        NODE_ID = CONFIG__NODE_ID;
        LOGFILE = CONFIG__LOGFILE
    #elif defined(ARDUINO_ARCH_SAMD)
        if (readSDConfig(CONFIG_FILE) > 0)
            fail(FAILED_CONFIG_FILE_READ);
        NETWORK_ID = (uint32_t)(atoi(getConfig("NETWORK_ID")));
        NODE_ID = (uint32_t)(atoi(getConfig("NODE_ID")));
        LOGFILE = getConfig("LOGFILE");

        #if (SSD1306_LCDHEIGHT != 32)
          #error("Height incorrect, please fix Adafruit_SSD1306.h!");
        #endif
        display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
        display.display();
        delay(1000);
        display.clearDisplay();
        display.display();
        pinMode(BUTTON_A, INPUT_PULLUP);
        //pinMode(BUTTON_B, INPUT_PULLUP);
        pinMode(BUTTON_C, INPUT_PULLUP);
        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.setCursor(0,0);
        display.println("KnightLab SensorGrid .");
        display.display();
        for (int i=0; i<3; i++) {
          display.print(" .");
          display.display();
        }
        delay(2000);
        display.clearDisplay();
        display.display();

        setDate();

    #endif
    
    #if DUST_SENSOR
        //setupDustSensor();
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

    displayCurrentRTCDateTime();
}

#include "sensor_grid.h"
#include "display.h"

static RTC_PCF8523 rtc;

uint32_t NETWORK_ID;
uint32_t NODE_ID;
char* LOGFILE;
char* GPS_MODULE;
uint32_t DISPLAY_TIMEOUT;
uint32_t lastTransmit = 0;
uint32_t oledActivated = 0;
bool USE_OLED = true;
bool oledOn;

Adafruit_Si7021 sensorSi7021TempHumidity = Adafruit_Si7021();
Adafruit_SI1145 sensorSi1145UV = Adafruit_SI1145();
bool sensorSi7021Module = false;
bool sensorSi1145Module = false;
Adafruit_FeatherOLED display = Adafruit_FeatherOLED();

bool WiFiPresent = false;

void setup() {

    #ifdef DEBUG
        while (!Serial); // only do this if connected to USB
    #endif
    Serial.begin(9600);
    Serial.println(F("SRL RDY"));

    /* The Adafruit RTCLib API and related tutorials are quite misleading here.
     * ::begin() simply calls Wire.begin() and returns true. See:
     * https://github.com/adafruit/RTClib/blob/e03a97139e285eeb4a5a3c052ab421f53a88031c/RTClib.cpp#L355
     *
     * rtc.begin() will not work (will hang) if RTC is not connected. To make
     * logger optional (not sure if this is really a goal), would need to provide
     * alt begin. E.g. from bhelterline https://github.com/adafruit/RTClib/issues/64

     boolean RTC_DS1307::begin(void) {
         Wire.begin();
         Wire.beginTransmission(DS1307_ADDRESS);
         Wire._I2C_WRITE((byte)0);
         if ( Wire.endTransmission() == 0 ) {
             return true;
         }
         return false;
     }

     * But with PCF8523. However note that this example, and general RTC
     * library code uses Wire1
     */
    Serial.println(F("Starting RTC"));
    rtc.begin(); // Always true. Don't check as per Adafruit tutorials
    if (! rtc.initialized()) {
        /*
         * Compiled time, thus compile and upload immediately. Arduino IDE
         * generally re-compiles for an upload anyway.
         * Pull battery to reset the clock
         */
        Serial.println(F("Init RTC to compile time: "));
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    flashLED(2, HIGH);


    if (readSDConfig(CONFIG_FILE) > 0)
        fail(FAILED_CONFIG_FILE_READ);
    NETWORK_ID = (uint32_t)(atoi(getConfig("NETWORK_ID")));
    NODE_ID = (uint32_t)(atoi(getConfig("NODE_ID")));
    LOGFILE = getConfig("LOGFILE");
    DISPLAY_TIMEOUT = (uint32_t)(atoi(getConfig("DISPLAY_TIMEOUT", "60")));
    GPS_MODULE = getConfig("GPS_MODULE");

    if (USE_OLED) {
        Serial.print(F("Display timeout set to: ")); Serial.println(DISPLAY_TIMEOUT);
        setupDisplay();
        oledOn = true;
    }

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

    if (GPS_MODULE) {
        setupGPS();
    } else {
        Serial.println(F("No GPS_MODULE specified in config .. Skipping GPS setup"));
    }
    setupRadio();
    WiFiPresent = setupWiFi(getConfig("WIFI_SSID"), getConfig("WIFI_PASS"));

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
    Serial.println(F("****"));
    printRam();

    if (USE_OLED) {
        if (oledOn) {
            display.setBattery(batteryLevel());
            display.renderBattery();
        }
        if ( (DISPLAY_TIMEOUT > 0) && oledOn && ((millis() - oledActivated) > (OLED_TIMEOUT * 1000L)) ) {
            oledOn = false;
            display.clearDisplay();
            display.clearMsgArea();
            display.display();
        }
        if (oledOn) {
            updateDateTimeDisplay();
        } else {
            if (! digitalRead(BUTTON_C)) { // there seems to be a conflict on button A (pin 9)
                oledOn = true;
                oledActivated = millis();
                display.setBattery(batteryLevel());
                display.renderBattery();
                displayCurrentRTCDateTime();
            }
        }
    }

    if (CHARGE_ONLY) {
      Serial.print(F("BAT: ")); Serial.println(batteryLevel());
      delay(10000);
      return;
    }

    if ( TRANSMIT && (millis() - lastTransmit) > 1000 * 10) {
        Serial.println(F("***\nTX\n---"));
        transmit();
        Serial.println(F("Transmitted"));
        lastTransmit = millis();
    }
    // RX as soon as possible after TX to catch ack of sent msg
    if (RECEIVE) {
        Serial.println(F("***\nRX\n---"));
        receive();
    }
}

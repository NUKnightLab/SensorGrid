#include "SensorGrid.h"
#include "display.h"

/* Config defaults are strings so they can be passed to getConfig */
static const char* DEFAULT_NETWORK_ID = "1";
static const char* DEFAULT_NODE_ID = "1";
static const char* DEFAULT_RF95_FREQ = "915.0";  // for U.S.
static const char* DEFAULT_TX_POWER = "10";
static const char* DEFAULT_PROTOCOL_VERSION = "0.11";
static const char* DEFAULT_DISPLAY_TIMEOUT = "60";
static const char* DEFAULT_OLED = "0";

static const bool CHARGE_ONLY = false;
static const bool TRANSMIT = true;
static const bool RECEIVE = true;

/* vars set by config file */
uint32_t networkID;
uint32_t nodeID;
float rf95Freq;
uint8_t txPower;
float protocolVersion;
char* logfile;
char* gpsModule;
uint32_t displayTimeout;
uint8_t hasOLED;

uint32_t lastTransmit = 0;
uint32_t oledActivated = 0;
bool oledOn;

RTC_PCF8523 rtc;
Adafruit_Si7021 sensorSi7021TempHumidity = Adafruit_Si7021();
Adafruit_SI1145 sensorSi1145UV = Adafruit_SI1145();
bool sensorSi7021Module = false;
bool sensorSi1145Module = false;
Adafruit_FeatherOLED display = Adafruit_FeatherOLED();

bool WiFiPresent = false;

void setup() {

    if (true) {
        while (!Serial); // only do this if connected to USB
    }
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

    if (!readSDConfig(CONFIG_FILE)) {
        networkID = (uint32_t)(atoi(getConfig("NETWORK_ID")));
        nodeID = (uint32_t)(atoi(getConfig("NODE_ID")));
        rf95Freq = (float)(atof(getConfig("RF95_FREQ")));
        txPower = (uint8_t)(atoi(getConfig("TX_POWER")));
        protocolVersion = (float)(atof(getConfig("PROTOCOL_VERSION")));
        logfile = getConfig("LOGFILE");
        displayTimeout = (uint32_t)(atoi(getConfig("DISPLAY_TIMEOUT", "60")));
        gpsModule = getConfig("GPS_MODULE");
        hasOLED = (uint8_t)(atoi(getConfig("DISPLAY")));
    } else {
        Serial.println(F("Using default configs"));
        networkID = (uint32_t)(atoi(DEFAULT_NETWORK_ID));
        nodeID = (uint32_t)(atoi(DEFAULT_NODE_ID));
        rf95Freq = (float)(atof(DEFAULT_RF95_FREQ));
        txPower = (uint8_t)(atoi(DEFAULT_TX_POWER));
        protocolVersion = (float)(atof(DEFAULT_PROTOCOL_VERSION));
        displayTimeout = (uint32_t)(atoi(DEFAULT_DISPLAY_TIMEOUT));
        hasOLED = (uint8_t)(atoi(DEFAULT_OLED));
    }

    if (hasOLED) {
        Serial.print(F("Display timeout set to: ")); Serial.println(displayTimeout);
        setupDisplay();
        oledOn = true;
    }

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

    if (gpsModule) {
        setupGPS();
    } else {
        Serial.println(F("No GPS_MODULE specified in config .. Skipping GPS setup"));
    }
    setupRadio();
    char* ssid = getConfig("WIFI_SSID");
    if (ssid) {
      Serial.println(F("ssid is valid"));
      WiFiPresent = setupWiFi(getConfig("WIFI_SSID", ""), getConfig("WIFI_PASS", ""));
    } else {
      Serial.println(F("ssid is null"));
      WiFiPresent = false;
    }

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

    if (hasOLED) {
        if (oledOn) {
            display.setBattery(batteryLevel());
            display.renderBattery();
        }
        if ( (displayTimeout > 0) && oledOn && ((millis() - oledActivated) > (displayTimeout * 1000L)) ) {
            oledOn = false;
            display.clearDisplay();
            display.clearMsgArea();
            display.display();
        }
        if (oledOn) {
            updateDateTimeDisplay();
        }
        if (! digitalRead(BUTTON_C)) { // there seems to be a conflict on button A (pin 9)
            if (oledOn) { // wait for possible shutdown request
                long start = millis();
                while (!digitalRead(BUTTON_C)) {
                    if ( (millis() - start) < 3000) {
                        display.clearDisplay();
                        display.clearMsgArea();
                        display.setCursor(0,16);
                        display.print("Shutdown OK");
                        display.display();
                        while(1);
                    }
                    delay(500);
                }
            } else {
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

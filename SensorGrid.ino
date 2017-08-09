#include "SensorGrid.h"
#include "display.h"
#include <pt.h>

#define NODE_TYPE_COLLECTOR 1
#define NODE_TYPE_ROUTER 2
#define NODE_TYPE_SENSOR 3
#define NODE_TYPE_ORDERED_COLLECTOR 4
#define NODE_TYPE_ORDERED_SENSOR_ROUTER 5

/* Config defaults are strings so they can be passed to getConfig */
static const char* DEFAULT_NETWORK_ID = "1";
static const char* DEFAULT_NODE_ID = "1";
static const char* DEFAULT_RF95_FREQ = "915.0";  // for U.S.
static const char* DEFAULT_TX_POWER = "10";
static const char* DEFAULT_PROTOCOL_VERSION = "0.11";
static const char* DEFAULT_DISPLAY_TIMEOUT = "60";
static char* DEFAULT_COLLECTOR_ID = "1";
static char* DEFAULT_OLED = "0";
static char* DEFAULT_LOG_FILE = "sensorgrid.log";
static char* DEFAULT_TRANSMIT = "1";
static char* DEFAULT_LOG_MODE = "NODE"; // NONE, NODE, NETWORK, ALL
uint8_t SHARP_GP2Y1010AU0F_DUST_PIN;
uint8_t GROVE_AIR_QUALITY_1_3_PIN;

static const bool CHARGE_ONLY = false;
static const bool RECEIVE = true;

/* vars set by config file */
uint32_t networkID;
uint32_t nodeID;
float rf95Freq;
uint8_t txPower;
uint8_t nodeType;
uint32_t collectorID;
uint16_t protocolVersion;
char* logfile;
char* logMode;
char* gpsModule;
uint32_t displayTimeout;
uint8_t hasOLED;
uint8_t doTransmit;

uint32_t lastTransmit = 0;
uint32_t lastReTransmit = 0;
uint32_t oledActivated = 0;
uint32_t cButtonPressed = 0;
bool oledOn;

char *nodeIds[254] = {0};

RTC_PCF8523 rtc;
Adafruit_Si7021 sensorSi7021TempHumidity = Adafruit_Si7021();
Adafruit_SI1145 sensorSi1145UV = Adafruit_SI1145();
bool sensorSi7021Module = false;
bool sensorSi1145Module = false;
Adafruit_FeatherOLED display = Adafruit_FeatherOLED();

bool WiFiPresent = false;
uint32_t lastDisplayTime = 0;

static struct pt pt1, pt2, pt3, pt4;

static int radioTransmitThread(struct pt *pt, int interval) {
  static unsigned long timestamp = 0;
  PT_BEGIN(pt);
  while(1) { // never stop 
    /* each time the function is called the second boolean
    *  argument "millis() - timestamp > interval" is re-evaluated
    *  and if false the function exits after that. */
    PT_WAIT_UNTIL(pt, millis() - timestamp > interval );
    transmit();
    timestamp = millis();
  }
  PT_END(pt);
}

float bat = 0.0;
uint32_t displayTime = 0;

static void _updateDisplayBattery() {
     if (batteryLevel() != bat) {
        display.setCursor(45,0);
        display.fillRect(77, 0, 51, 7, BLACK);
        bat = batteryLevel();
        display.setBattery(bat);
        display.renderBattery();
        display.display();         
    }    
}

static int updateDisplayBatteryThread(struct pt *pt, int interval) {
  static unsigned long timestamp = 0;
  
  PT_BEGIN(pt);
  while(1) { // never stop 
    /* each time the function is called the second boolean
    *  argument "millis() - timestamp > interval" is re-evaluated
    *  and if false the function exits after that. */
    PT_WAIT_UNTIL(pt, millis() - timestamp > interval );
    if (oledOn)
        _updateDisplayBattery();
    timestamp = millis();
  }
  PT_END(pt);
}

static void _updateDisplay() {
    DateTime now = rtc.now();
    now = DateTime(now.year(),now.month(),now.day(),now.hour(),now.minute(),0);
    if (now.unixtime() != displayTime) {
        display.clearMsgArea();
        displayTime = displayCurrentRTCDateTime();
        updateGPSDisplay();
        display.display();
    }     
}

static int updateDisplayThread(struct pt *pt, int interval) {
  static unsigned long timestamp = 0;
  
  PT_BEGIN(pt);
  while(1) { // never stop 

    /* each time the function is called the second boolean
    *  argument "millis() - timestamp > interval" is re-evaluated
    *  and if false the function exits after that. */
    PT_WAIT_UNTIL(pt, millis() - timestamp > interval );
    if (oledOn)
        _updateDisplay();
    timestamp = millis();
  }
  PT_END(pt);
}

static int displayTimeoutShutdownThread(struct pt *pt, int interval) {
  static unsigned long timestamp = 0;
  
  PT_BEGIN(pt);
  while(1) { // never stop 

    /* each time the function is called the second boolean
    *  argument "millis() - timestamp > interval" is re-evaluated
    *  and if false the function exits after that. */
    PT_WAIT_UNTIL(pt, millis() - timestamp > interval );

    if (! digitalRead(BUTTON_C)) {
        if (!oledOn) {
            oledOn = true;
            oledActivated = millis();
            displayTime = 0; // force immediate update - don't wait for next minute
            _updateDisplay();
            _updateDisplayBattery();
            displayID();
        }
        if (cButtonPressed == 0) {
            cButtonPressed = millis();
        } else {
            if ( (millis() - cButtonPressed) > 3000) {
                display.clearDisplay();
                display.setCursor(0,16);
                display.print("Shutdown OK");
                display.display();
                while(1);
            }
        }
    } else {
        if (cButtonPressed > 0)
            cButtonPressed = 0;
        if ( displayTimeout > 0 && (millis() - oledActivated) > displayTimeout*1000) {
            oledOn = false;
            display.clearDisplay();
            display.display();
        }
    }
    timestamp = millis();
  }
  PT_END(pt);
}

void setup() {

    randomSeed(analogRead(A0));
    
    if (false) {
        while (!Serial); // only do this if connected to USB
    }
    Serial.begin(9600);
    Serial.println(F("SRL RDY"));
    Serial.print("Message length: "); Serial.println(sizeof(Message), DEC);

    /* The Adafruit RTCLib API and related tutorials are misleading here.
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
    //Serial.println(F("Starting GPS"));
    //setupGPS();
    //Serial.println(F("Starting RTC"));
    //rtc.begin(); // Always true. Don't check as per Adafruit tutorials
    
    flashLED(2, HIGH);

    if (!readSDConfig(CONFIG_FILE)) {
        networkID = (uint32_t)(atoi(getConfig("NETWORK_ID")));
        nodeID = (uint32_t)(atoi(getConfig("NODE_ID")));
        rf95Freq = (float)(atof(getConfig("RF95_FREQ")));
        txPower = (uint8_t)(atoi(getConfig("TX_POWER")));
        protocolVersion = atof(getConfig("PROTOCOL_VERSION"));
        logfile = getConfig("LOGFILE", DEFAULT_LOG_FILE);
        logMode = getConfig("LOGMODE", DEFAULT_LOG_MODE);
        displayTimeout = (uint32_t)(atoi(getConfig("DISPLAY_TIMEOUT", "60")));
        gpsModule = getConfig("GPS_MODULE");
        hasOLED = (uint8_t)(atoi(getConfig("DISPLAY", DEFAULT_OLED)));
        doTransmit = (uint8_t)(atoi(getConfig("TRANSMIT", DEFAULT_TRANSMIT)));
        nodeType = (uint8_t)(atoi(getConfig("NODE_TYPE")));
        collectorID = (uint32_t)(atoi(getConfig("COLLECTOR_ID", DEFAULT_COLLECTOR_ID)));
        SHARP_GP2Y1010AU0F_DUST_PIN = (uint8_t)(atoi(getConfig("SHARP_GP2Y1010AU0F_DUST_PIN")));
        GROVE_AIR_QUALITY_1_3_PIN = (uint8_t)(atoi(getConfig("GROVE_AIR_QUALITY_1_3_PIN")));

        if (nodeType == NODE_TYPE_ORDERED_COLLECTOR) {
            char* nodeIdsConfig = strdup(getConfig("ORDERED_NODE_IDS", ""));
            if (nodeIdsConfig[0] == NULL) {
                Serial.println("BAD CONFIGURATION. Node type ORDERED_COLLECTOR requires ORDERED_NODE_IDS");
                while(1);
            }
            char **ap;
            Serial.print("Separating string: "); Serial.println(nodeIdsConfig);
            for (ap = nodeIds; (*ap = strsep(&nodeIdsConfig, ",")) != NULL;)
                if (**ap != '\0')
                    if (++ap >= &nodeIds[10])
                        break;
            Serial.println("Node IDs:");
            for (int i=0; i<254 && nodeIds[i] != NULL; i++) {
                if (i > 0)
                    Serial.print(",");
                Serial.print(nodeIds[i]);
            }
            Serial.println("");
        }
    } else {
        Serial.println(F("Using default configs"));
        networkID = (uint32_t)(atoi(DEFAULT_NETWORK_ID));
        nodeID = (uint32_t)(atoi(DEFAULT_NODE_ID));
        rf95Freq = (float)(atof(DEFAULT_RF95_FREQ));
        txPower = (uint8_t)(atoi(DEFAULT_TX_POWER));
        protocolVersion = (float)(atof(DEFAULT_PROTOCOL_VERSION));
        logfile = DEFAULT_LOG_FILE;
        logMode = DEFAULT_LOG_MODE;
        displayTimeout = (uint32_t)(atoi(DEFAULT_DISPLAY_TIMEOUT));
        hasOLED = (uint8_t)(atoi(DEFAULT_OLED));
        doTransmit = (uint8_t)(atoi(DEFAULT_TRANSMIT));
        nodeType = (uint8_t)(atoi(getConfig("NODE_TYPE")));
        collectorID = (uint32_t)(atoi(DEFAULT_COLLECTOR_ID));        
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

    if (gpsModule && nodeType != NODE_TYPE_ROUTER && nodeType != NODE_TYPE_COLLECTOR) {
        setupGPS();
    } else {
        Serial.println(F("No GPS_MODULE specified in config .. Skipping GPS setup"));
    }
    rtc.begin();
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

    if (SHARP_GP2Y1010AU0F_DUST_PIN) {
        setupDustSensor();
    }

    if (GROVE_AIR_QUALITY_1_3_PIN) {
        setupGroveAirQualitySensor();
    }

    if (sizeof(Message) > RH_RF95_MAX_MESSAGE_LEN) {
        fail(MESSAGE_STRUCT_TOO_LARGE);
    }
    Serial.println(F("OK!"));

    PT_INIT(&pt1);  // initialise the two
    PT_INIT(&pt2);  // protothread variables
    PT_INIT(&pt3);
    PT_INIT(&pt4);
}

void loop() {

    if (CHARGE_ONLY) {
      Serial.print(F("BAT: ")); Serial.println(batteryLevel());
      delay(10000);
      return;
    }

    Serial.println(F("****"));
    printRam();

    if (nodeType == NODE_TYPE_ROUTER || nodeType == NODE_TYPE_COLLECTOR) {
        receive();
    } else if (nodeType == NODE_TYPE_SENSOR) {
        radioTransmitThread(&pt1, 10*1000);
    } else if (nodeType == NODE_TYPE_ORDERED_SENSOR_ROUTER) {
        waitForInstructions();
    } else if (nodeType == NODE_TYPE_ORDERED_COLLECTOR) {
        uint32_t nextCollectTime = millis() + 60000;
        for (int i=0; i<254 && nodeIds[i] != NULL; i++) {
            collectFromNode(atoi(nodeIds[i]), nextCollectTime);
        }
        if (nextCollectTime > millis()) {
            delay(nextCollectTime - millis());
        } else {
            Serial.println("WARNING: cycle period time too short to collect all node data!!!");
        }
    } else {
        Serial.print("Unknown node type: "); Serial.println(nodeType, DEC);
        while(1);
    }

    if (hasOLED) {
        updateDisplayThread(&pt2, 1000);
        updateDisplayBatteryThread(&pt3, 10 * 1000);
        displayTimeoutShutdownThread(&pt4, 50);
    }
    
    /*
    int gpsYear = GPS.year;
    if (gpsYear != 0 && gpsYear != 80) {
        uint32_t gpsTime = DateTime(GPS.year,GPS.month,GPS.day,GPS.hour,GPS.minute,GPS.seconds).unixtime();
        uint32_t rtcTime = rtc.now().unixtime();
        if (rtcTime - gpsTime > 1 || gpsTime - rtcTime > 1) { 
          Serial.println("Setting RTC to GPS time");
          rtc.adjust(DateTime(GPS.year,GPS.month,GPS.day,GPS.hour,GPS.minute,GPS.seconds));
        }
    } */
}

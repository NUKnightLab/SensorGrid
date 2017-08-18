#include "SensorGrid.h"
#include "config.h"
#include "display.h"
#include <pt.h>

uint8_t SHARP_GP2Y1010AU0F_DUST_PIN;
uint8_t GROVE_AIR_QUALITY_1_3_PIN;
uint32_t oledActivated = 0;
bool oledOn;

RTC_PCF8523 rtc;
Adafruit_Si7021 sensorSi7021TempHumidity = Adafruit_Si7021();
Adafruit_SI1145 sensorSi1145UV = Adafruit_SI1145();
bool sensorSi7021Module = false;
bool sensorSi1145Module = false;
Adafruit_FeatherOLED display = Adafruit_FeatherOLED();

bool WiFiPresent = false;
uint32_t display_time = 0;

volatile int aButtonState = 0;
volatile int bButtonState = 0;
volatile int cButtonState = 0;

/* 
 * protothreads (https://github.com/fernandomalmeida/protothreads-arduino) 
 */

static struct pt radio_transmit_protothread;
static struct pt update_display_protothread;
static struct pt update_display_battery_protothread;
static struct pt display_timeout_shutdown_protothread;

static int radioTransmitThread(struct pt *pt, int interval)
{
  static unsigned long timestamp = 0;
  PT_BEGIN(pt);
  while(1) { // never stop 
    PT_WAIT_UNTIL(pt, millis() - timestamp > interval );
    transmit();
    timestamp = millis();
  }
  PT_END(pt);
}

static int updateDisplayBatteryThread(struct pt *pt, int interval)
{
  static unsigned long timestamp = 0;
  
  PT_BEGIN(pt);
  while(1) { // never stop 
    PT_WAIT_UNTIL(pt, millis() - timestamp > interval );
    if (oledOn)
        updateDisplayBattery();
    timestamp = millis();
  }
  PT_END(pt);
}

static int updateDisplayThread(struct pt *pt, int interval)
{
  static unsigned long timestamp = 0;
  
  PT_BEGIN(pt);
  while(1) { // never stop 
    PT_WAIT_UNTIL(pt, millis() - timestamp > interval );
    if (oledOn)
        updateDisplay();
    timestamp = millis();
  }
  PT_END(pt);
}

static int displayTimeoutShutdownThread(struct pt *pt, int interval)
{
  static unsigned long timestamp = 0;
  
  PT_BEGIN(pt);
  while(1) { // never stop 
    PT_WAIT_UNTIL(pt, millis() - timestamp > interval );
    if ( config.display_timeout > 0 && (millis() - oledActivated) > config.display_timeout*1000) {
        oledOn = false;
        display.clearDisplay();
        display.display();
    }
    timestamp = millis();
  }
  PT_END(pt);
}

/*
 * interrupts
 */
 
void aButton_ISR()
{
    aButtonState = !digitalRead(BUTTON_A);
    if (aButtonState) {
        oledOn = !oledOn;
        if (oledOn) {
            oledActivated = millis();
            display_time = 0; // force immediate update - don't wait for next minute
            updateDisplay();
            displayID();
            //updateDisplayBattery();
        } else {
            display.clearDisplay();
            display.display();
        }
    }
}


/*
 * setup and loop
 */
 
void setup()
{

    randomSeed(analogRead(A0));
    
    if (true) {
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
    //flashLED(2, HIGH);
    load_config();
    Serial.println("Config loaded");
    Serial.print("Node type: "); Serial.println(config.node_type);
    pinMode(LED, OUTPUT);
    pinMode(RFM95_RST, OUTPUT);
    pinMode(BUTTON_A, INPUT_PULLUP);
    pinMode(BUTTON_B, INPUT_PULLUP);
    pinMode(BUTTON_C, INPUT_PULLUP);  
    digitalWrite(LED, LOW);
    digitalWrite(RFM95_RST, HIGH);
    
    if (config.has_oled) {
        Serial.print(F("Display timeout set to: ")); Serial.println(config.display_timeout);
        setupDisplay();
        oledOn = true;
        attachInterrupt(BUTTON_A, aButton_ISR, CHANGE);
    }

    Serial.print(F("BAT: "));
    if (VBATPIN == A7) {
        Serial.println(F("A7"));
    } else {
        Serial.println(F("A9"));
    }
    if (config.charge_only) {
      return;
    }
    
    if (config.gps_module && config.node_type != NODE_TYPE_ROUTER && config.node_type != NODE_TYPE_COLLECTOR) {
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

    /* initialize protothreads */
    PT_INIT(&radio_transmit_protothread);
    if (config.has_oled) {
        PT_INIT(&update_display_protothread);
        PT_INIT(&update_display_battery_protothread);
        PT_INIT(&display_timeout_shutdown_protothread);
    }
}

void loop()
{
    if (config.charge_only) {
      Serial.print(F("BAT: ")); Serial.println(batteryLevel());
      if (config.has_oled && oledOn)
          updateDisplayBattery();
      delay(10000);
      return;
    }

    Serial.println(F("****"));
    printRam();

    if (config.node_type == NODE_TYPE_ROUTER || config.node_type == NODE_TYPE_COLLECTOR) {
        receive();
    } else if (config.node_type == NODE_TYPE_SENSOR) {
        radioTransmitThread(&radio_transmit_protothread, 10*1000);
    } else if (config.node_type == NODE_TYPE_ORDERED_SENSOR_ROUTER) {
        waitForInstructions();
    } else if (config.node_type == NODE_TYPE_ORDERED_COLLECTOR) {
        uint32_t nextCollectTime = millis() + 60000;
        for (int i=0; i<254 && config.node_ids[i] != NULL; i++) {
            Serial.print("----- COLLECT FROM NODE ID: "); Serial.println(config.node_ids[i], DEC);
            collectFromNode(config.node_ids[i], nextCollectTime);
        }
        if (nextCollectTime > millis()) {
            delay(nextCollectTime - millis());
        } else {
            Serial.println("WARNING: cycle period time too short to collect all node data!!!");
        }
    } else {
        Serial.print("Unknown node type: "); Serial.println(config.node_type, DEC);
        while(1);
    }

    if (config.has_oled) {
        updateDisplayThread(&update_display_protothread, 1000);
        updateDisplayBatteryThread(&update_display_battery_protothread, 10 * 1000);
        displayTimeoutShutdownThread(&display_timeout_shutdown_protothread, 1000);
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

#include "SensorGrid.h"
#include "config.h"
#include "display.h"
#include <pt.h>

static uint32_t oled_activated_time = 0;
bool oled_is_on;

RTC_PCF8523 rtc;
Adafruit_FeatherOLED display = Adafruit_FeatherOLED();

bool WiFiPresent = false;
uint32_t display_clock_time = 0;

volatile int aButtonState = 0;
volatile int bButtonState = 0;
volatile int cButtonState = 0;
static bool shutdown_requested = false;
WiFiClient client;

/* 
 * protothreads (https://github.com/fernandomalmeida/protothreads-arduino) 
 */

static struct pt radio_transmit_protothread;
static struct pt update_clock_protothread;
static struct pt update_display_protothread;
static struct pt update_display_battery_protothread;
static struct pt display_timeout_protothread;

static int radioTransmitThread(struct pt *pt, int interval)
{
  static unsigned long timestamp = 0;
  PT_BEGIN(pt);
  while(1) { // never stop 
    PT_WAIT_UNTIL(pt, millis() - timestamp > interval );
    sendCurrentMessage(config.collector_id);
    timestamp = millis();
  }
  PT_END(pt);
}

static int updateClockThread(struct pt *pt, int interval)
{
  static unsigned long timestamp = 0;
  PT_BEGIN(pt);
  while(1) { // never stop 
    PT_WAIT_UNTIL(pt, millis() - timestamp > interval );
    int gps_year = GPS.year;
    if (gps_year != 0 && gps_year != 80) {
        uint32_t gps_time = DateTime(GPS.year,GPS.month,GPS.day,GPS.hour,GPS.minute,GPS.seconds).unixtime();
        uint32_t rtc_time = rtc.now().unixtime();
        if (rtc_time - gps_time > 1 || gps_time - rtc_time > 1) { 
            rtc.adjust(DateTime(GPS.year,GPS.month,GPS.day,GPS.hour,GPS.minute,GPS.seconds));
        }
    }
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
    if (oled_is_on)
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
    if (oled_is_on)
        updateDisplay();
        shutdown_requested = false;
    timestamp = millis();
  }
  PT_END(pt);
}

static int displayTimeoutThread(struct pt *pt, int interval)
{
  static unsigned long timestamp = 0;
  PT_BEGIN(pt);
  while(1) { // never stop 
    PT_WAIT_UNTIL(pt, millis() - timestamp > interval );
    if ( config.display_timeout > 0 && (millis() - oled_activated_time) > config.display_timeout*1000) {
        oled_is_on = false;
        shutdown_requested = false;
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
        oled_is_on = !oled_is_on;
        if (oled_is_on) {
            oled_activated_time = millis();
            display_clock_time = 0; // force immediate update - don't wait for next minute
            updateDisplay();
            displayID();
            updateDisplayBattery();
            shutdown_requested = false;
        } else {
            display.clearDisplay();
            display.display();
        }
    }
}

void bButton_ISR()
{
    if (shutdown_requested) {
        display.clearDisplay();
        display.setCursor(0,24);
        display.print("Shutdown OK");
        display.display();
        while(1);
    }
}

void cButton_ISR()
{
    if (shutdown_requested) {
        shutdown_requested = false;
        display.fillRect(0, 24, 128, 29, BLACK);
        display.display();
    } else {
        shutdown_requested = true;
        display.fillRect(0, 24, 128, 29, BLACK);
        display.setCursor(0,24);
        display.print("B:Shutdown   C:Cancel");
        display.display();
    }
}

/*
 * setup and loop
 */
 
void setup()
{
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

    loadConfig();
    Serial.print("SD_CHIP_SELECT_PIN: ");
    Serial.println(config.SD_CHIP_SELECT_PIN);
    Serial.print("RFM95_CS: ");
    Serial.println(config.RFM95_CS);
    Serial.print("RFM95_RST: ");
    Serial.println(config.RFM95_RST);
    Serial.print("RFM95_INT: ");
    Serial.println(config.RFM95_INT);



    Serial.print("Node type: "); Serial.println(config.node_type);
    pinMode(LED, OUTPUT);
    pinMode(config.RFM95_RST, OUTPUT);
    pinMode(BUTTON_A, INPUT_PULLUP);
    pinMode(BUTTON_B, INPUT_PULLUP);
    pinMode(BUTTON_C, INPUT_PULLUP);  
    digitalWrite(LED, LOW);
    digitalWrite(config.RFM95_RST, HIGH);
    
    if (config.has_oled) {
        Serial.print(F("Display timeout set to: ")); Serial.println(config.display_timeout);
        setupDisplay();
        oled_is_on = true;
        attachInterrupt(BUTTON_A, aButton_ISR, CHANGE);
        attachInterrupt(BUTTON_B, bButton_ISR, FALLING);
        attachInterrupt(BUTTON_C, cButton_ISR, FALLING);
    }

    if (config.charge_only) {
      return;
    }
    
    rtc.begin();
    setupRadio();

    if (config.wifi_password) {
        Serial.print("Attempting to connect to Wifi: ");
        Serial.print(config.wifi_ssid);
        Serial.print(" With password: ");
        Serial.println(config.wifi_password);
        connectToServer(client, config.wifi_ssid, config.wifi_password, config.api_host, config.api_port); 
        WiFiPresent = true; // TODO: can we set this based on success of connect?
    } else {
        Serial.println("No WiFi configuration found");
    }
    
    setupSensors();
    
    if (sizeof(Message) > RH_RF95_MAX_MESSAGE_LEN) {
        fail(MESSAGE_STRUCT_TOO_LARGE);
    }
    Serial.println(F("OK!"));

    /* initialize protothreads */
    PT_INIT(&radio_transmit_protothread);
    PT_INIT(&update_clock_protothread);
    if (config.has_oled) {
        PT_INIT(&update_display_protothread);
        PT_INIT(&update_display_battery_protothread);
        PT_INIT(&display_timeout_protothread);
    }
}

void loop()
{
    if (config.charge_only) {
        Serial.print(F("BAT: ")); Serial.println(batteryLevel());
        if (config.has_oled && oled_is_on)
            updateDisplayBattery();
        delay(10000);
        return;
    }

    if (config.node_type == NODE_TYPE_ROUTER || config.node_type == NODE_TYPE_COLLECTOR) {
        receive();
    } else if (config.node_type == NODE_TYPE_SENSOR) {
        radioTransmitThread(&radio_transmit_protothread, 10*1000);
    } else if (config.node_type == NODE_TYPE_ORDERED_SENSOR_ROUTER) {
        waitForInstructions();
    } else if (config.node_type == NODE_TYPE_ORDERED_COLLECTOR) {
        uint32_t nextCollectTime = millis() + (config.collection_period*1000);
        for (int i=0; i<254 && config.node_ids[i] != NULL; i++) {
            Serial.print("----- COLLECT FROM NODE ID: "); Serial.println(config.node_ids[i], DEC);
            collectFromNode(config.node_ids[i], nextCollectTime, client, config.wifi_ssid);
        }
        if (nextCollectTime > millis()) {
            broadcastSleep(nextCollectTime);
            delay(nextCollectTime - millis());
        } else {
            Serial.println("WARNING: cycle period time too short to collect all node data!!!");
        } 
    } else {
        Serial.print("Unknown node type: "); Serial.println(config.node_type, DEC);
        while(1);
    }

    updateClockThread(&update_clock_protothread, 1000);

    if (config.has_oled) {
        updateDisplayThread(&update_display_protothread, 1000);
        updateDisplayBatteryThread(&update_display_battery_protothread, 10 * 1000);
        displayTimeoutThread(&display_timeout_protothread, 1000);
    }
}

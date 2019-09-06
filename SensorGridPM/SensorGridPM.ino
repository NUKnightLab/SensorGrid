/**
 * Knight Lab SensorGrid
 *
 * Wireless air pollution (Particulate Matter PM2.5 & PM10) monitoring over LoRa radio
 * 
 * Copyright 2018 Northwestern University
 */
//#include <KnightLab_GPS.h>
#include "config.h"
#include "runtime.h"
#include <LoRa.h>
#include <LoRaHarvest.h>

#define SET_CLOCK false
#define SERIAL_TIMEOUT 10000

WatchdogType Watchdog;

RTC_PCF8523 rtc;
int start_epoch;
OLED oled = OLED(rtc);

/* local utilities */


static void setupLogging() {
    Serial.begin(115200);
    set_logging(true);
}

static void setRTC() {
  DateTime dt = DateTime(start_epoch);
  rtc.adjust(DateTime(start_epoch - 18000)); // Subtract 5 hours to adjust for timezone
}

static void setRTCz() {
    DateTime dt = rtc.now();
    rtcz.setDate(dt.day(), dt.month(), dt.year()-2000); // When setting the year to 2018, it becomes 34
    rtcz.setTime(dt.hour(), dt.minute(), dt.second());
}

static void printCurrentTime() {
    DateTime dt = rtc.now();
    logln(F("Current time: %02d:%02d:%02d, "), rtcz.getHours(), rtcz.getMinutes(), rtcz.getSeconds());
    logln(F("Current Epoch: %u"), rtcz.getEpoch());
}

/* end local utilities */

/*
 * interrupts
 */

void aButton_ISR() {
    static bool busy = false;
    if (busy) return;
    busy = true;
    // rtcz.disableAlarm();
    static volatile int state = 0;
    state = !digitalRead(BUTTON_A);
    if (state) {
        logln(F("A-Button pushed"));
        oled.toggleDisplayState();
    }
    if (oled.isOn()) {
      //updateClock(); // Temporarily removed
      oled.displayDateTime();
    } else {
      oled.clear();
    }
    busy = false;
    // rtcz.disableAlarm();
    /*
    aButtonState = !digitalRead(BUTTON_A);
    if (aButtonState) {
        _on = !_on;
        if (_on) {
            _activated_time = millis();
            displayDateTimeOLED();
        } else {
            standbyOLED();
        }
    }
    */
}

/*
void updateClock() {
    int gps_year = GPS.year;
    if (gps_year != 0 && gps_year != 80) {
        uint32_t gps_time = DateTime(GPS.year, GPS.month, GPS.day, GPS.hour, GPS.minute, GPS.seconds).unixtime();
        uint32_t rtc_time = rtc.now().unixtime();
        if (rtc_time - gps_time > 1 || gps_time - rtc_time > 1) {
            rtc.adjust(DateTime(GPS.year, GPS.month, GPS.day, GPS.hour, GPS.minute, GPS.seconds));
        }
    }
    setRTCz();
}
*/

void setupSensors() {
    pinMode(12, OUTPUT);  // enable pin to HPM boost
    Serial.println("Loading sensor config ...");
    loadSensorConfig(nodeId(), getTime);
    Serial.println(".. done loading sensor config");
}

void setupClocks() {
    rtc.begin();
    /* In general, we no longer use SET_CLOCK. Instead use a GPS module to set the time */
    if (SET_CLOCK) {
        log_(F("Printing initial DateTime: "));
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        log_(F(__DATE__));
        log_(F(' '));
        logln(F(__TIME__));
    }
    rtcz.begin();
    setRTCz();
}

/* hard fault handler */

/* see Segger debug manual: https://www.segger.com/downloads/application-notes/AN00016 */
static volatile unsigned int _Continue;
void HardFault_Handler(void) {
     _Continue = 0u;
    //
    // When stuck here, change the variable value to != 0 in order to step out
    //
    logln(F("!!!!**** HARD FAULT -- REQUESTING RESET *****!!!!"));
    SCB->AIRCR = 0x05FA0004;  // System reset
    while (_Continue == 0u) {}
}

// Doesn't include watchdog like the one in runtime
static void standbyTEMP() {
  if (DO_STANDBY) {
    rtcz.standbyMode();
  }
}
// Doesn't include interrupt function like in runtime
void setInterruptTimeoutTEMP(DateTime &datetime) {
    rtcz.setAlarmSeconds(datetime.second());
    rtcz.setAlarmMinutes(datetime.minute());
    rtcz.enableAlarm(rtcz.MATCH_MMSS);
}

void waitSerial()
{
    unsigned long _start = millis();
    while ( !Serial && (millis() - _start) < SERIAL_TIMEOUT ) {}
}

void setup() {
    waitSerial();
    rtcz.begin();
    systemStartTime(rtcz.getEpoch());
    Watchdog.enable();
    config.loadConfig();
    nodeId(config.node_id);
    setupSensors();
    setupLoRa(config.RFM95_CS, config.RFM95_RST, config.RFM95_INT);
    setupRunner();
    for (uint8_t i=0; i<255; i++) {
      txPower(i, DEFAULT_LORA_TX_POWER);
    }
    Watchdog.disable();
    recordRestart();
}

void loop() {
    Watchdog.enable();
    runRunner();

    /**
     * Some apparent lockups may actually be indefinite looping on STANDBY status
     * if for some reason the scheduled interrupt never happens to kick us out of
     * STANDBY mode. For that reason, the standby_timer will track time and should
     * never get to be longer than the scheduled heartbeat period.
     */
    //static uint32_t standby_timer = getTime();
}

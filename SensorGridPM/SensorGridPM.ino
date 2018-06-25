/**
 * Knight Lab SensorGrid
 *
 * Wireless air pollution (Particulate Matter PM2.5 & PM10) monitoring over LoRa radio
 */
#include <KnightLab_GPS.h>
#include "config.h"
#include "lora.h"
#include "runtime.h"
#include "oled.h"


WatchdogSAMD Watchdog;

#define SET_CLOCK false

enum Mode mode = WAIT;

RTCZero rtcz;
RTC_PCF8523 rtc;
OLED oled = OLED(rtc);

// uint32_t sampleRate = 10; // sample rate of the sine wave in Hertz, how many times per second the TC5_Handler() function gets called per second basically
// int MAX_TIMEOUT = 10;
// unsigned long sample_period = 60 * 10;
// unsigned long heartbeat_period = 30;
// unsigned long next_sample;
// int sensor_init_time = 7;

/* local utilities */

static void setupLogging()
{
    Serial.begin(115200);
    set_logging(true);
}

static void setRTCz()
{
    DateTime dt = rtc.now();
    rtcz.setDate(dt.day(), dt.month(), dt.year());
    rtcz.setTime(dt.hour(), dt.minute(), dt.second());
}

static void printCurrentTime()
{
    Serial.print("Current time: ");
    Serial.print(rtcz.getHours());
    Serial.print(":");
    Serial.print(rtcz.getMinutes());
    Serial.print(":");
    Serial.println(rtcz.getSeconds());
    Serial.println(rtcz.getEpoch());
}

uint32_t getTime()
{
    return rtcz.getEpoch();
}

/* end local utilities */

/*
 * interrupts
 */

void aButton_ISR()
{
    static bool busy = false;
    if (busy) return;
    busy = true;
    rtcz.disableAlarm();
    static volatile int state = 0;
    state = !digitalRead(BUTTON_A);\
    if (state) {
        Serial.println("A-Button pushed");
        oled.toggleDisplayState();
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

void updateClock()
{
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

void setupHoneywell()
{
    pinMode(12, OUTPUT);  // enable pin to HPM boost
    HONEYWELL_HPM::setup(config.node_id, 0, &getTime);
    delay(2000);
    HONEYWELL_HPM::stop();
}

void setupClocks()
{
    rtc.begin();
    /* In general, we no longer use SET_CLOCK. Instead use a GPS module to set the time */
    if (SET_CLOCK) {
        Serial.print("Printing initial DateTime: ");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        Serial.print(F(__DATE__));
        Serial.print(' ');
        Serial.println(F(__TIME__));
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
    Serial.println("!!!!**** HARD FAULT -- REQUESTING RESET *****!!!!");
    SCB->AIRCR = 0x05FA0004;  // System reset
    while (_Continue == 0u){};
}

/* setup and loop */

void setup()
{
    setupGPS();
    setupClocks();
    oled.init();
    oled.displayStartup();
    /* This is causing lock-up. Need to do further research into low power modes
       on the Cortex M0 */
    // oled.setButtonFunction(BUTTON_A, *aButton_ISR, CHANGE);
    // oled.displayDateTime();
    unsigned long _start = millis();
    while ( !Serial && (millis() - _start) < WAIT_SERIAL_TIMEOUT ){};
    if (ALWAYS_LOG || Serial) {
        setupLogging();
    }
    logln(F("begin setup .."));
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    loadConfig();
    setupRadio(config.RFM95_CS, config.RFM95_INT, config.node_id);
    radio->sleep();
    setupHoneywell();
    ADAFRUIT_SI7021::setup();
    // This is done in RTCZero::standbyMode
    // https://github.com/arduino-libraries/RTCZero/blob/master/src/RTCZero.cpp
    // SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);
    logln(F(".. setup complete"));
    printCurrentTime();
    oled.endDisplayStartup();
}

void loop()
{
    // enable (instead of reset) Watchdog every loop because we disable during standby
    Watchdog.enable();
    static uint32_t start_time = getTime();
    static uint32_t uptime;
    uptime = millis();
    static uint32_t next_collection_time = getNextCollectionTime();
    if (start_time && getTime() - start_time > 3 * 60) {
        oled.off();
    }
    if (oled.isOn()) {
        updateClock();
        oled.displayDateTime();
    }
    if (mode == WAIT) {
        setInitTimeout();
    } else if (mode == INIT) {
        initSensors();
        setSampleTimeout();
    } else if (mode == SAMPLE) {
        recordDataSamples();
        if (getTime() > next_collection_time) {
            setCommunicateDataTimeout();
        } else {
            mode = WAIT;
        }
    } else if (mode == COMMUNICATE) {
        recordBatteryLevel();
        recordUptime(uptime);
        if (DO_LOG_DATA) {
            logData(!DO_TRANSMIT_DATA);
        }
        if (DO_TRANSMIT_DATA) {
            transmitData(true);
        }
        next_collection_time = getNextCollectionTime();
        mode = WAIT;
    } else if (mode == HEARTBEAT) {
        flashHeartbeat();
        mode = WAIT;
    }
}

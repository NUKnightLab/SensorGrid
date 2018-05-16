/**
 * Knight Lab SensorGrid
 *
 * Wireless air pollution (Particulate Matter PM2.5 & PM10) monitoring over LoRa radio
 */
#include "config.h"
#include "lora.h"
#include "runtime.h"
#include "oled.h"

#define SET_CLOCK false

enum Mode mode = WAIT;

RTCZero rtcz;
RTC_PCF8523 rtc;
OLED oled = OLED(rtc);


uint32_t sampleRate = 10; //sample rate of the sine wave in Hertz, how many times per second the TC5_Handler() function gets called per second basically
int MAX_TIMEOUT = 10;
unsigned long sample_period = 60 * 10;
unsigned long heartbeat_period = 30;
unsigned long next_sample;
int sensor_init_time = 10;

/* local utilities */

static void setup_logging()
{
    Serial.begin(115200);
    set_logging(true);
}

static void set_rtcz()
{
    DateTime dt = rtc.now();
    rtcz.setDate(dt.day(), dt.month(), dt.year());
    rtcz.setTime(dt.hour(), dt.minute(), dt.second());
}

static void current_time()
{
    Serial.print("Current time: ");
    Serial.print(rtcz.getHours());
    Serial.print(":");
    Serial.print(rtcz.getMinutes());
    Serial.print(":");
    Serial.println(rtcz.getSeconds());
    Serial.println(rtcz.getEpoch());
}

uint32_t get_time()
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
    //rtcz.disableAlarm();
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
/* setup and loop */

void setup()
{
    rtc.begin();
    if (SET_CLOCK) {
        Serial.print("Printing initial DateTime: ");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        Serial.print(F(__DATE__));
        Serial.print(' ');
        Serial.println(F(__TIME__));
    }
    rtcz.begin();
    set_rtcz();
    //initOLED(rtc);
    oled.init();
    /* This is causing lock-up. Need to do further research into low power modes
       on the Cortex M0 */
    //oled.setButtonFunction(BUTTON_A, *aButton_ISR, CHANGE);
    oled.displayDateTime();
    //displayDateTimeOLED();

    unsigned long _start = millis();
    while ( !Serial && (millis() - _start) < WAIT_SERIAL_TIMEOUT );
    if (ALWAYS_LOG || Serial) {
        setup_logging();
    }
    logln(F("begin setup .."));
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    loadConfig();
    setup_radio(config.RFM95_CS, config.RFM95_INT, config.node_id);
    //startTimer(10);
    HONEYWELL_HPM::setup(config.node_id, 0, &get_time);
    delay(2000);
    HONEYWELL_HPM::stop();
    logln(F(".. setup complete"));
    current_time();
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);
}

void loop()
{
    static uint32_t start_time = get_time();
    if (start_time && get_time() - start_time > 3 * 60)
        oled.off();
    if (mode == WAIT) {
        set_init_timeout();
    } else if (mode == INIT) {
        init_sensors();
        set_sample_timeout();
    } else if (mode == SAMPLE) {
        record_data_samples();
        set_communicate_data_timeout();
    } else if (mode == COMMUNICATE) {
        communicate_data();
        mode = WAIT;
    } else if (mode == HEARTBEAT) {
        flash_heartbeat();
        mode = WAIT;
    }
    oled.displayDateTime();
}

/**
 * Knight Lab SensorGrid
 *
 * Wireless air pollution (Particulate Matter PM2.5 & PM10) monitoring over LoRa radio
 */
#include "config.h"
#include "lora.h"
#include "runtime.h"

enum Mode mode = WAIT;

RTCZero rtcz;
RTC_PCF8523 rtc;

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

/* setup and loop */

void setup()
{
    rtc.begin();
    rtcz.begin();
    set_rtcz();
    HONEYWELL_HPM::setup(0, &get_time);
    unsigned long _start = millis();
    while ( !Serial && (millis() - _start) < WAIT_SERIAL_TIMEOUT );
    if (ALWAYS_LOG || Serial) {
        setup_logging();
    }
    logln(F("begin setup .."));
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    load_config();
    setup_radio();
    //startTimer(10);
    logln(F(".. setup complete"));
    current_time();
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);
}

void loop()
{
    if (mode == WAIT) {
        set_init_timeout();
        return;
    } else if (mode == INIT) {
        init_sensors();
        set_sample_timeout();
        return;
    } else if (mode == SAMPLE) {
        record_data_samples();
        set_communicate_data_timeout();
        return;
    } else if (mode == COMMUNICATE) {
        communicate_data();
        mode = WAIT;
        return;
    } else if (mode == HEARTBEAT) {
        flash_heartbeat();
        mode = WAIT;
        return;
    }
}

/**
 * Copyright 2018 Northwestern University
 */
#include "config.h"
#include "runtime.h"
#include "HONEYWELL_HPM.h"
#include <ArduinoJson.h>
#include "lora.h"

static char data[100] = "somedata";

/* local utils */
void _writeToSD(char* filename, char* str)
{
    static SdFat sd;
    Serial.print(F("Init SD card .."));
    if (!sd.begin(config.SD_CHIP_SELECT_PIN)) {
          Serial.println(F(" .. SD card init failed!"));
          return;
    }
    if (false) {  // true to check available SD filespace
        Serial.print(F("SD free: "));
        uint32_t volFree = sd.vol()->freeClusterCount();
        float fs = 0.000512*volFree*sd.vol()->blocksPerCluster();
        Serial.println(fs);
    }
    File file;
    Serial.print(F("Writing log line to ")); Serial.println(filename);
    file = sd.open(filename, O_WRITE|O_APPEND|O_CREAT); //will create file if it doesn't exist
    file.println(str);
    file.close();
    Serial.println(F("File closed"));
}

void writeToSD(char* filename, char* str)
{
    digitalWrite(8, HIGH);
    _writeToSD(filename, str);
    digitalWrite(8, LOW);
}

static void write_data(char* buf)
{
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(buf);
    if (!root.success()) {
        logln(F("Could not parse data from sensor"));
    } else {
        char str[100];
        uint32_t ts = root["ts"];
        DateTime t = DateTime(ts);
        float bat = batteryLevel();
        int pm25 = root["data"][0];
        int pm10 = root["data"][1];
        sprintf(str, "%i-%02d-%02dT%02d:%02d:%02d,%d.%02d,%d,%d",
            t.year(), t.month(), t.day(), t.hour(), t.minute(), t.second(),
            (int)bat, (int)(bat*100)%100, pm25, pm10);
        writeToSD("datalog.txt", str);
    }
}
    /*
    File dataFile = SD.open("datalog.txt", FILE_WRITE);
    if (dataFile) {
        Serial.println("Opening dataFile...");
    } else {
        Serial.println("error opening datalog.txt");
    }
    DateTime now = rtc.now();
    dataFile.print(now.year());
    dataFile.print("-");
    dataFile.print(now.month());
    dataFile.print("-");
    dataFile.print(now.day());
    dataFile.print("T");
    dataFile.print(now.hour());
    dataFile.print(":");
    dataFile.print(now.minute());
    dataFile.print(":");
    dataFile.print(now.second());
    dataFile.print(",");
    dataFile.print(batteryLevel());
    //dataFile.print(",");
    //dataFile.print(uartbuf[3]*256 + uartbuf[4]); // PM 2.5
    //dataFile.print(",");
    //dataFile.println(uartbuf[5]*256+uartbuf[6]); // PM 10
    Serial.println("Closing dataFile");
    dataFile.close();
    */

static uint32_t next_period_time(int period)
{
    uint32_t t = rtcz.getEpoch();
    int d = t % period;
    return (t + period - d);
}

static void standby()
{
    mode = STANDBY;
    if (DO_STANDBY)
        rtcz.standbyMode();
}

/* end local utils */

/* runtime mode interrupts */

void init_sensors_INT()
{
    logln(F("Setting Mode: INIT"));
    mode = INIT;
}

void record_data_samples_INT()
{
    logln(F("Setting Mode: SAMPLE"));
    mode = SAMPLE;
}

void heartbeat_INT()
{
    logln(F("Setting Mode: HEARTBEAT"));
    mode = HEARTBEAT;
}

void communicate_data_INT()
{
    logln(F("Setting Mode: COMMUNICATE"));
    mode = COMMUNICATE;
}

/* end runtime mode interrupts */

/* runtime mode timeouts */

void set_communicate_data_timeout()
{
    logln(F("set_communicate_data_timeout"));
    uint32_t prev_sample = next_period_time(SAMPLE_PERIOD*60) - SAMPLE_PERIOD*60;
    uint32_t com = prev_sample + 30;
    DateTime dt = DateTime(com);
    rtcz.setAlarmSeconds(dt.second());
    rtcz.setAlarmMinutes(dt.minute());
    rtcz.enableAlarm(rtcz.MATCH_MMSS);
    rtcz.attachInterrupt(communicate_data_INT);
    standby();
}

void set_init_timeout() {
    log_(F("Check set_init_timeout: "));
    uint32_t sample = next_period_time(SAMPLE_PERIOD*60);
    uint32_t init = sample - 10;
    uint32_t heartbeat = next_period_time(30);
    logln(F("Next init time is %d"), init);
    logln(F("Next heartbeat time is %d"), heartbeat);
    if (heartbeat < init - 2) {
        DateTime dt = DateTime(heartbeat);
        rtcz.setAlarmSeconds(dt.second());
        rtcz.enableAlarm(rtcz.MATCH_SS);
        rtcz.attachInterrupt(heartbeat_INT);
        println(F("heartbeat %02d:%02d"), dt.minute(), dt.second());
    } else {
        DateTime dt = DateTime(init);
        rtcz.setAlarmSeconds(dt.second());
        rtcz.setAlarmMinutes(dt.minute());
        rtcz.enableAlarm(rtcz.MATCH_MMSS);
        rtcz.attachInterrupt(init_sensors_INT);
        println(F("init %02d:%02d"), dt.minute(), dt.second());
    }
    standby();
}

void set_sample_timeout()
{
    logln(F("set_sample_timeout"));
    uint32_t sample = next_period_time(SAMPLE_PERIOD*60);
    uint32_t heartbeat = next_period_time(30);
    if (heartbeat < sample - 2) {
        rtcz.setAlarmSeconds(DateTime(heartbeat).second());
        rtcz.enableAlarm(rtcz.MATCH_SS);
        rtcz.attachInterrupt(heartbeat_INT);
    } else {
        DateTime dt = DateTime(sample);
        rtcz.setAlarmSeconds(dt.second());
        rtcz.setAlarmMinutes(dt.minute());
        rtcz.enableAlarm(rtcz.MATCH_MMSS);
        rtcz.attachInterrupt(record_data_samples_INT);
    }
    standby();
}

/* end runtime mode timeouts */

/* runtime mode handlers */

void init_sensors()
{
    logln(F("Init sensor for data sampling"));
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on
    delay(1000);
    digitalWrite(LED_BUILTIN, LOW);    // turn the LED off
    HONEYWELL_HPM::start();
}

void record_data_samples()
{
    logln(F("Taking data sample"));
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on
    delay(1000);
    digitalWrite(LED_BUILTIN, LOW);    // turn the LED off
    delay(1000);
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on
    delay(1000);
    digitalWrite(LED_BUILTIN, LOW);    // turn the LED off
    char buf[100];
    HONEYWELL_HPM::read(buf, 100);
    Serial.println(buf);
    delay(2000);
    if (HONEYWELL_HPM::stop()) {
        Serial.println("Sensor fan stopped");
        digitalWrite(LED_BUILTIN, HIGH);
        delay(500);
        digitalWrite(LED_BUILTIN, LOW);
        delay(500);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(500);
        digitalWrite(LED_BUILTIN, LOW);
        delay(500);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(1000);
        digitalWrite(LED_BUILTIN, LOW);
    } else {
        Serial.println("Sensor fan did not stop");
        digitalWrite(LED_BUILTIN, HIGH);
        delay(500);
        digitalWrite(LED_BUILTIN, LOW);
        delay(500);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(500);
        digitalWrite(LED_BUILTIN, LOW);
        delay(500);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(500);
        digitalWrite(LED_BUILTIN, LOW);
        delay(1000);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(500);
        digitalWrite(LED_BUILTIN, LOW);
        delay(500);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(500);
        digitalWrite(LED_BUILTIN, LOW);
        delay(500);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(500);
        digitalWrite(LED_BUILTIN, LOW);
    }
    write_data(buf);
}

void flash_heartbeat()
{
    logln(F("Heartbeat"));
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);    // turn the LED off
    delay(100);
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);    // turn the LED off
}

void communicate_data()
{
    logln(F("Communicating"));
    static Message msg = {
        .sensorgrid_version = config.sensorgrid_version,
        .network_id = 4,
        .from_node = 2,
        .message_type = 2,
        .len = 0
    };
    memcpy(msg.data, data, strlen(data));
    send_message(&msg, sizeof(msg), config.collector_id);
    /*
    if (RECV_STATUS_SUCCESS == receive(msg, 60000)) {
        Serial.println("Runtime received message");
        Serial.print("VERSION: ");
        Serial.println(msg->sensorgrid_version, DEC);
        Serial.print("NEWORK ID: ");
        Serial.println(msg->network_id, DEC);
        Serial.print("FROM NODE: ");
        Serial.println(msg->from_node, DEC);
        Serial.print("MESSAGE TYPE: ");
        Serial.println(msg->message_type, DEC);
    } else {
        Serial.println("Runtime did not receive message.");
    }
    */
    /*
    int limit = random(30, 90);
    uint32_t now = rtcz.getEpoch();
    while (rtcz.getEpoch() < now + limit) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(50);
        digitalWrite(LED_BUILTIN, LOW);
        delay(50);
    }
    */
}

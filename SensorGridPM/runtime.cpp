/**
 * Copyright 2018 Northwestern University
 */
#include "config.h"
#include "runtime.h"
#include "HONEYWELL_HPM.h"
#include <ArduinoJson.h>
#include "lora.h"
#include <avr/dtostrf.h>

static uint8_t msg_buf[140] = {0};
static Message *msg = (Message*)msg_buf;
static char databuf[100] = {0};
StaticJsonBuffer<200> json_buffer;
JsonArray& data_array = json_buffer.createArray();

/* data history */
struct DataSample {
    char data[100];
    struct DataSample *next;
};


DataSample *head = NULL;
DataSample *tail = NULL;

DataSample *appendData()
{
    DataSample *new_sample = (DataSample*)malloc(sizeof(DataSample));
    if (new_sample == NULL) {
        logln(F("Error creating new sample"));
        while(1);
    }
    if (head == NULL) {
        head = new_sample;
    }
    if (tail != NULL) {
        tail->next = new_sample;
    }
    tail = new_sample;
    tail->next = NULL;
    return tail;
}

/* -- end data history */

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

/*
static void write_data(const char *buf)
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
        int pm25 = root["hpm"][0];
        int pm10 = root["hpm"][1];
        sprintf(str, "%i-%02d-%02dT%02d:%02d:%02d,%d.%02d,%d,%d",
            t.year(), t.month(), t.day(), t.hour(), t.minute(), t.second(),
            (int)bat, (int)(bat*100)%100, pm25, pm10);
        writeToSD("datalog.txt", str);
    }
}
*/

static uint32_t getNextPeriodTime(int period)
{
    uint32_t t = rtcz.getEpoch();
    int d = t % period;
    return (t + period - d);
}

uint32_t getNextCollectionTime()
{
    int period = config.collection_period;
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

void setCommunicateDataTimeout()
{
    logln(F("setCommunicateDataTimeout"));
    uint32_t prev_sample = getNextPeriodTime(config.sample_period) - config.sample_period;
    /* TODO: we end up in a bad state if com time is not far enough out for data
       sampling to complete. This is theoretically possible with the current HPM
       collection scheme. How to handle this while still having predictable collection
       times? */
    uint32_t com = prev_sample + 30;
    DateTime dt = DateTime(com);
    rtcz.setAlarmSeconds(dt.second());
    rtcz.setAlarmMinutes(dt.minute());
    rtcz.enableAlarm(rtcz.MATCH_MMSS);
    rtcz.attachInterrupt(communicate_data_INT);
    standby();
}

void setInitTimeout() {
    log_(F("Check setInitTimeout: "));
    uint32_t sample = getNextPeriodTime(config.sample_period);
    uint32_t init = sample - INIT_LEAD_TIME;
    uint32_t heartbeat = getNextPeriodTime(config.heartbeat_period);
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

void setSampleTimeout()
{
    logln(F("setSampleTimeout"));
    uint32_t sample = getNextPeriodTime(config.sample_period);
    uint32_t heartbeat = getNextPeriodTime(config.heartbeat_period);
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

void initSensors()
{
    logln(F("Init sensor for data sampling"));
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on
    delay(1000);
    digitalWrite(LED_BUILTIN, LOW);    // turn the LED off
    digitalWrite(12, HIGH);
    HONEYWELL_HPM::start();
}

void throwawayHPMData()
{
    memset(databuf, 0, 100);
    Serial.println("Reading throwaway HPM data");
    digitalWrite(12, HIGH);
    HONEYWELL_HPM::read(databuf, 100);
    if (HONEYWELL_HPM::stop()) {
        digitalWrite(12, LOW);
        Serial.println("Sensor fan stopped");
    } else {
        Serial.println("Sensor fan did not stop");
    }
}

void logData(bool clear)
{
    Serial.println("LOGGING DATA: ------");
    DataSample *cursor = head;
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
    Serial.print(F("Writing log lines to "));
    Serial.println("datalog.txt");
    file = sd.open("datalog.txt", O_WRITE|O_APPEND|O_CREAT); //will create file if it doesn't exist
    while (cursor != NULL) {
        Serial.println(cursor->data);
        file.println(cursor->data);
        if (clear) {
            DataSample *_cursor = cursor;
            cursor = cursor->next;
            head = cursor;
            free(_cursor);
        } else {
            cursor = cursor->next;
        }
    }
    char str[20];
    float bat = batteryLevel();
    sprintf(str, "{\"bat\":%d.%02d,\"ts\":%ld}", (int)bat, (int)(bat*100)%100,
        rtcz.getEpoch());
    file.println(str);
    Serial.println("-------");
    file.close();
    Serial.println(F("File closed"));
}

void recordDataSamples()
{
    logln(F("Taking data sample"));
    memset(databuf, 0, 100);
    digitalWrite(12, HIGH);
    //HONEYWELL_HPM::read(databuf, 100);
    Serial.println("Before readData:");
    //data_array.printTo(Serial);
    //logln(databuf);
    DataSample *sample = appendData();
    //HONEYWELL_HPM::readData(data_array);
    HONEYWELL_HPM::readDataSample(sample->data, 100);
    //char buf[100];
    //HONEYWELL_HPM::read(buf, 100);
    //memset(msg->data, 0, 100);
    //msg->data[0] = '[';
    //msg->len = HONEYWELL_HPM::read(&msg->data[1], 99) + 1;
    //Serial.println(msg->data);
    //logln(msg->data);
    delay(2000);
    if (HONEYWELL_HPM::stop()) {
        digitalWrite(12, LOW);
        Serial.println("Sensor fan stopped");
    } else {
        Serial.println("Sensor fan did not stop");
    }
    //log_("Writing data: "); println(msg->data);
    //write_data((const char*)msg->data);
    //log_("Writing data: "); println(databuf);
    //write_data(databuf);
    //log_("Data is written. Current msg data: ");
    //println(msg->data);
}

void flashHeartbeat()
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

void communicateData()
{
    logln(F("Communicating"));
    float battery = batteryLevel();
    if (battery <= 3.6) {
        Serial.println("Critical Battery Level. Not communicating data.");
        return;
    }
    char bat[4];
    dtostrf(battery, 3, 2, bat);
    //static char *data = "somedata";
    //static uint8_t buf[140] = {0};
    //static Message *msg = (Message*)buf;
    msg->sensorgrid_version = config.sensorgrid_version;
    msg->network_id = config.network_id;
    msg->from_node = config.node_id;
    msg->message_type = 2;
    //msg->len = 0;
    //const char *data = "somedata\0";
    Serial.println("memcopying data");
    //memcpy(&msg->data, &data, sizeof(data));
    //strcpy(msg->data, data);
    //Serial.print("Copied: "); Serial.println(msg->data);
    //Serial.print("sizeof msg: "); Serial.println(sizeof(Message), DEC);
    //Serial.print("sizeof data: "); Serial.println(sizeof(data), DEC);
    //Serial.print("strlen data: "); Serial.println(strlen(data), DEC);
    //for (int i=0; i<(5 + msg->len); i++) {
    //    Serial.print(msg_buf[i], HEX); Serial.print(" ");
    //}
    Serial.println("");
    //send_message(msg_buf, 5 + strlen(msg->data) + 1, config.collector_id);
    memset(msg->data, 0, 100);
    sprintf(&msg->data[0], "[");
    sprintf(&msg->data[1], databuf);
    //float bat = batteryLevel();
 
    Serial.print("bat: "); Serial.println(bat);
    sprintf(&msg->data[1+strlen(databuf)], ",{\"node\":%d,\"bat\":%s}", config.node_id, bat);
    sprintf(&msg->data[strlen(msg->data)], "]");
    //sprintf(&msg->data[msg->len], ",{\"bat\":\"%.2f\"}]", batteryLevel());
    msg->len = strlen(msg->data);
    send_message(msg_buf, 5 + msg->len, config.collector_id);
    radio->sleep();
    log_("Sent message: "); print(msg->data);
    print(" len: "); println("%d", msg->len);
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

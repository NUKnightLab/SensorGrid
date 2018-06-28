/**
 * Copyright 2018 Northwestern University
 */
#include "config.h"
#include "runtime.h"
#include <ArduinoJson.h>
#include "lora.h"
#include <avr/dtostrf.h>

static uint8_t msg_buf[140] = {0};
static Message *msg = reinterpret_cast<Message*>(msg_buf);
static char databuf[100] = {0};
StaticJsonBuffer<200> json_buffer;
JsonArray& data_array = json_buffer.createArray();

/* data history */
struct DataSample {
    char data[DATASAMPLE_DATASIZE];
    struct DataSample *next;
};


DataSample *head = NULL;
DataSample *tail = NULL;

DataSample *appendData() {
    DataSample *new_sample = reinterpret_cast<DataSample*>(malloc(sizeof(DataSample)));
    if (new_sample == NULL) {
        logln(F("Error creating new sample"));
        while (1) {}
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

void _writeToSD(char* filename, char* str) {
    static SdFat sd;
    log_(F("Init SD card .."));
    if (!sd.begin(config.SD_CHIP_SELECT_PIN)) {
          println(F(" .. SD card init failed!"));
          return;
    }
    if (false) {  // true to check available SD filespace
        log_(F("SD free: "));
        uint32_t volFree = sd.vol()->freeClusterCount();
        float fs = 0.000512*volFree*sd.vol()->blocksPerCluster();
        println(F("%.2f"), fs);
    }
    File file;
    logln(F("Writing log line to %s"), filename);
    file = sd.open(filename, O_WRITE|O_APPEND|O_CREAT);  // will create file if it doesn't exist
    file.println(str);
    file.close();
    logln(F("File closed"));
}

void writeToSD(char* filename, char* str) {
    digitalWrite(8, HIGH);
    _writeToSD(filename, str);
    digitalWrite(8, LOW);
}

static uint32_t getNextPeriodTime(int period) {
    uint32_t t = rtcz.getEpoch();
    int d = t % period;
    return (t + period - d);
}

uint32_t getNextCollectionTime() {
    int period = config.collection_period;
    uint32_t t = rtcz.getEpoch();
    int d = t % period;
    return (t + period - d);
}

static void standby() {
    mode = STANDBY;
    if (DO_STANDBY) {
        Watchdog.disable();
        rtcz.standbyMode();
    }
}

typedef void (*InterruptFunction)();

void setInterruptTimeout(DateTime &datetime, InterruptFunction fcn) {
    rtcz.setAlarmSeconds(datetime.second());
    rtcz.setAlarmMinutes(datetime.minute());
    rtcz.enableAlarm(rtcz.MATCH_MMSS);
    rtcz.attachInterrupt(fcn);
}

/* end local utils */

/* runtime mode interrupts */

void initSensors_INT() {
    logln(F("Setting Mode: INIT"));
    mode = INIT;
}

void recordDataSamples_INT() {
    logln(F("Setting Mode: SAMPLE"));
    mode = SAMPLE;
}

void heartbeat_INT() {
    logln(F("Setting Mode: HEARTBEAT"));
    mode = HEARTBEAT;
}

void communicateData_INT() {
    logln(F("Setting Mode: COMMUNICATE"));
    mode = COMMUNICATE;
}

/* end runtime mode interrupts */

/* runtime mode timeouts */

void setCommunicateDataTimeout() {
    logln(F("setCommunicateDataTimeout"));
    uint32_t prev_sample = getNextPeriodTime(config.sample_period) - config.sample_period;
    /* TODO: we end up in a bad state if com time is not far enough out for data
       sampling to complete. This is theoretically possible with the current HPM
       collection scheme. How to handle this while still having predictable collection
       times? */
    uint32_t com = prev_sample + 30;
    DateTime dt = DateTime(com);
    setInterruptTimeout(dt, communicateData_INT);
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
        setInterruptTimeout(dt, heartbeat_INT);
        println(F("heartbeat %02d:%02d"), dt.minute(), dt.second());
    } else {
        DateTime dt = DateTime(init);
        setInterruptTimeout(dt, initSensors_INT);
        println(F("init %02d:%02d"), dt.minute(), dt.second());
    }
    standby();
}

void setSampleTimeout() {
    logln(F("setSampleTimeout"));
    uint32_t sample = getNextPeriodTime(config.sample_period);
    uint32_t heartbeat = getNextPeriodTime(config.heartbeat_period);
    if (heartbeat < sample - 2) {
        DateTime dt = DateTime(heartbeat);
        setInterruptTimeout(dt, heartbeat_INT);
    } else {
        DateTime dt = DateTime(sample);
        setInterruptTimeout(dt, recordDataSamples_INT);
    }
    standby();
}

/* end runtime mode timeouts */

/* runtime mode handlers */

void initSensors() {
    logln(F("Init sensor for data sampling"));
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on
    digitalWrite(12, HIGH);
    Watchdog.reset();
    HONEYWELL_HPM::start();
}

void recordBatteryLevel() {
    //float bat = batteryLevel();
    char bat[5];
    ftoa(batteryLevel(), bat, 2);
    DataSample *batSample = appendData();
    snprintf(batSample->data, DATASAMPLE_DATASIZE, "{\"node\":%d,\"bat\":%s,\"ts\":%lu}",
        config.node_id, bat, rtcz.getEpoch());
}

void recordTempAndHumidity() {
    DataSample *dataSample = appendData();
    ADAFRUIT_SI7021::read(dataSample->data, DATASAMPLE_DATASIZE);
}

void recordUptime(uint32_t uptime) {
    DataSample *sample = appendData();
    snprintf(sample->data, DATASAMPLE_DATASIZE, "{\"node\":%d,\"uptime\":%lu,\"ts\":%lu}",
        config.node_id, uptime, rtcz.getEpoch());
}

void logData(bool clear) {
    logln(F("\nLOGGING DATA: ------"));
    DataSample *cursor = head;
    static SdFat sd;
    log_(F("Init SD card .."));
    if (!sd.begin(config.SD_CHIP_SELECT_PIN)) {
          println(F(" .. SD card init failed!"));
          return;
    }
    if (false) {  // true to check available SD filespace
        /* TODO: does this work? */
        log_(F("SD free: "));
        uint32_t volFree = sd.vol()->freeClusterCount();
        float fs = 0.000512*volFree*sd.vol()->blocksPerCluster();
        println(F("%.2f"), fs);
    }
    File file;
    logln(F("Writing log lines to datalog.txt"));
    file = sd.open("datalog.txt", O_WRITE|O_APPEND|O_CREAT);  // will create file if it doesn't exist
    while (cursor != NULL) {
        logln(cursor->data);
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
    logln(F("-------"));
    file.close();
    logln(F("File closed"));
}

void transmitData(bool clear) {
    msg->sensorgrid_version = config.sensorgrid_version;
    msg->network_id = config.network_id;
    msg->from_node = config.node_id;
    msg->message_type = 2;
    memset(msg->data, 0, 100); // TODO: make this 100 a constant
    snprintf(&msg->data[0], MESSAGE_DATA_SIZE, "[");
    int data_index = 1;
    logln(F("TRANSMITTING DATA: ------"));
    DataSample *cursor = head;
    while (cursor != NULL) {
        Watchdog.reset();
        logln(cursor->data);
        if (data_index + strlen(cursor->data) >= MESSAGE_DATA_SIZE - 1) {  // TODO(Anyone): what is the real length we need to check?
            logln(F("Sending partial data history: "));
            msg->data[data_index-1] = ']';
            logln(msg->data);
            msg->len = strlen(msg->data);
            send_message(msg_buf, 5 + msg->len, config.collector_id);
            delay(5000);  // TODO(Anyone): better handling on the collector side?
            memset(msg->data, 0, 100);
            snprintf(&msg->data[0], MESSAGE_DATA_SIZE, "[");
            data_index = 1;
        }
        snprintf(&msg->data[data_index], MESSAGE_DATA_SIZE - data_index, cursor->data);  // data sample size - index
        data_index += strlen(cursor->data);
        msg->data[data_index++] = ',';
        if (clear) {
            DataSample *_cursor = cursor;
            cursor = cursor->next;
            head = cursor;
            free(_cursor);
        } else {
            cursor = cursor->next;
        }
    }
    logln(F("-------"));
    msg->data[data_index-1] = ']';
    msg->len = strlen(msg->data);
    logln(F("Sending message remainder"));
    Watchdog.reset();
    send_message(msg_buf, 5 + msg->len, config.collector_id);
    radio->sleep();
    log_(F("Sent message: ")); print(msg->data);
    print(F(" len: ")); println(F("%d"), msg->len);
}

void readDataSamples(){
    SensorConfig *cursor = sensor_config_head;
    while (cursor) {
        log_(F("Reading sensor %s .. "), cursor->id_str);
        DataSample *sample = appendData();
        cursor->read_function(sample->data, DATASAMPLE_DATASIZE);
        cursor->stop_function(); 
        logln(F("Sensor stopped: %s"), cursor->id_str);
        cursor = cursor->next;
    }
    digitalWrite(12,LOW);
}

void flashHeartbeat() {
    logln(F("Heartbeat"));
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);    // turn the LED off
    delay(100);
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);    // turn the LED off
}
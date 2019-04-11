/**
 * Copyright 2018 Northwestern University
 */
#include "config.h"
#include "runtime.h"
#include <ArduinoJson.h>
// #include "lora.h"
#include <avr/dtostrf.h>
//#include <KnightLab_LoRaUtils.h>
#include <KnightLab_RecordQueue.h>
#include <KnightLab_LoRa.h>

#define DATE_STRING_SIZE 11

static uint8_t msg_buf[KL_ROUTER_MAX_MESSAGE_LEN] = {0};
//static Message *msg = reinterpret_cast<Message*>(msg_buf);
StaticJsonBuffer<200> json_buffer;
JsonArray& data_array = json_buffer.createArray();

DataSample *datasample_head = NULL;
DataSample *datasample_tail = NULL;

DataSample *appendData() {
    DataSample *new_sample = reinterpret_cast<DataSample*>(malloc(sizeof(DataSample)));
    if (new_sample == NULL) {
        logln(F("Error creating new sample"));
        while (1) {}
    }
    if (datasample_head == NULL) {
        datasample_head = new_sample;
    }
    if (datasample_tail != NULL) {
        datasample_tail->next = new_sample;
    }
    datasample_tail = new_sample;
    datasample_tail->next = NULL;
    return datasample_tail;
}

/* -- end data history */

/* local utils */


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

void setInterruptTimeoutSched(DateTime &datetime) {
    rtcz.setAlarmSeconds(datetime.second());
    rtcz.setAlarmMinutes(datetime.minute());
    rtcz.enableAlarm(rtcz.MATCH_MMSS); 
}

static void standbySched() {
    if (DO_STANDBY) {
      Watchdog.disable();
      rtcz.standbyMode();
    } 
}

/* end local utils */

/* runtime mode interrupts */

void waitForBattery_INT() {
    Serial.println("Wake from wait for battery recharge");
    mode = WAIT;
}

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

void setWaitForBatteryTimeout(int t) {
    DateTime dt = DateTime(rtcz.getEpoch() + t);
    setInterruptTimeout(dt, waitForBattery_INT);
    standby();
}

void setCommunicateDataTimeout() {
    logln(F("setCommunicateDataTimeout"));
    uint32_t prev_sample = getNextPeriodTime(config.sample_period) - config.sample_period;
    /* TODO: we end up in a bad state if com time is not far enough out for data
       sampling to complete. This is theoretically possible with the current HPM
       collection scheme. How to handle this while still having predictable collection
       times? */
    uint32_t com = prev_sample + 30;
    DateTime dt = DateTime(com);
    if (com <= rtcz.getEpoch() + 1) {
        communicateData_INT();
        return;
    } else {
        setInterruptTimeout(dt, communicateData_INT);
        standby();
    }
}

/*
void setInitTimeout() {
    log_(F("Check setInitTimeout: "));
    uint32_t sample_time = getNextPeriodTime(config.sample_period);
    uint32_t init = sample_time - INIT_LEAD_TIME;
    uint32_t heartbeat_time = getNextPeriodTime(config.heartbeat_period);
    logln(F("Next init time is %lu"), init);
    logln(F("Next heartbeat time is %lu"), heartbeat_time);
    if (heartbeat_time < init - 2) {
        DateTime dt = DateTime(heartbeat_time);
        if (heartbeat_time <= rtcz.getEpoch() + 1) {
            heartbeat_INT();
            return;
        } else {
            setInterruptTimeout(dt, heartbeat_INT);
            // println(F("heartbeat %02d:%02d"), dt.minute(), dt.second());
            standby();
        }
    } else {
        DateTime dt = DateTime(init);
        if (init <= rtcz.getEpoch() + 1) {
            initSensors_INT();
            return;
        } else {
            setInterruptTimeout(dt, initSensors_INT);
            // println(F("init %02d:%02d"), dt.minute(), dt.second());
            standby();
        }
    }
}
*/

void setSampleTimeout() {
    logln(F("setSampleTimeout"));
    uint32_t sample_time = getNextPeriodTime(config.sample_period);
    uint32_t heartbeat_time = getNextPeriodTime(config.heartbeat_period);
    if (heartbeat_time < sample_time - 2) {
        DateTime dt = DateTime(heartbeat_time);
        if (heartbeat_time <= rtcz.getEpoch() + 1) {
            heartbeat_INT();
            return;
        } else {
            setInterruptTimeout(dt, heartbeat_INT);
            standby();
        }
    } else {
        DateTime dt = DateTime(sample_time);
        if (sample_time <= rtcz.getEpoch() + 1) {
            recordDataSamples_INT();
            return;
        } else {
            setInterruptTimeout(dt, recordDataSamples_INT);
            standby();
        }
    }
}

/* end runtime mode timeouts */

/* runtime mode handlers */

void initSensors() {
    Watchdog.enable();                                          // Enabled watchdog
    Serial.print("Uptime: ");
    Serial.println(millis());
    logln(F("Init sensor for data sampling"));
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on
    digitalWrite(12, HIGH); // turn on 5v power
    SensorConfig *sensor = sensor_config_head;
    while (sensor) {
        Watchdog.reset();
        log_(F("Starting sensor %s .. "), sensor->sensor->id);
        sensor->sensor->start();
        println(F("Started"));
        sensor = sensor->next;
    }
}

void stopSensors() {
    SensorConfig *sensor = sensor_config_head;
    while (sensor) {
        log_(F("Stopping sensor %s .. "), sensor->sensor->id);
        sensor->sensor->stop();
        println(F("Stopped"));
        sensor = sensor->next;
    }
    digitalWrite(12,LOW);
    digitalWrite(LED_BUILTIN, LOW);
}

/**
 * A 2-tiered thresholding mechanism is used to manage backing off of power usage
 * during low-battery times. When the low threshold is hit, the threshold is
 * changed to the high threshold setting until the battery is charged back up to
 * that value. After this recharge period, the threshold is set back to the lower
 * value. This approach prevents thrashing charge/discharge cycles at the threshold
 * border.
 *
 * While in the charge zone, no sampling or logging happens, in order to minimize
 * battery usage. How we handle communication and network robustness during this
 * time remains TBD. A heartbeat mechanism remains in place on the configured
 * heartbeat period. Early tests indicate even just this level of operation can be
 * a bit much for reliable recharge during development when standby mode is deactivated
 * for reliable Serial monitoring.
 * 
 * Note: this is extremely difficult to test and debug for. The general battery
 * charge/discharge profile looks significantly different in development than in
 * deployment. While developing, standby mode is generally deactivated, causing more
 * energy usage during "down" time. While USB charge rates during development and
 * testing are probably fairly consistent, solar charge rates in deployment can vary
 * drastically. Finally, the battery level read off the voltage divider on VBATPIN is
 * very inconsistent. Some robustness is built in with an averaging approach that
 * attempts to reject outliers, but it is not a panacea to the volatility seen in
 * these values. 
 */
static float batteryThreshold = 3.7;
static float batteryThresholdDelta = 0.2;
static float batteryThresholdLow = batteryThreshold;
static float batteryThresholdHigh = batteryThreshold + batteryThresholdDelta;
bool checkBatteryLevel() {
    Watchdog.enable();
    float bat = batteryLevelAveraged();
    log_(F("Battery level: "));
    Serial.println(bat);
    if (bat < batteryThreshold) {
        /* When we first hit the threshold, stop all the sensors */
        if (batteryThreshold < batteryThresholdHigh) {
            stopSensors();
        }
        Serial.print("Current battery threshold: ");
        Serial.println(batteryThreshold);
        logln(F("Warning: Low battery level. Waiting %d seconds."),
            config.heartbeat_period);
        batteryThreshold = batteryThresholdHigh;
        oled.off();
        flashHeartbeat();
        setWaitForBatteryTimeout(config.heartbeat_period);
        return false;
    } else {
        batteryThreshold = batteryThresholdLow;
        return true;
    }
}

void recordBatteryLevel() {
    // float bat = batteryLevel();
    char bat[5];
    ftoa(batteryLevelAveraged(), bat, 2);
    DataSample *batSample = appendData();
    snprintf(batSample->data, DATASAMPLE_DATASIZE, "{\"node\":%d,\"bat\":%s,\"ts\":%lu}",
        config.node_id, bat, rtcz.getEpoch());
}

void recordUptime(uint32_t uptime) {
    DataSample *sample = appendData();
    snprintf(sample->data, DATASAMPLE_DATASIZE, "{\"node\":%d,\"uptime\":%lu,\"ts\":%lu}",
        config.node_id, uptime, rtcz.getEpoch());
}

void logData() {
    recordBatteryLevel();                   // Should get a battery recording level before each log
    recordUptime(millis());
    Watchdog.enable();                    // Enabled watchdog

    logln(F("\nTRANSMITING DATA: ------"));
    //transmitData(false);

    logln(F("\nLOGGING DATA: ------"));
    DataSample *cursor = datasample_head;
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
    char datestring[11];
    DateTime dt = DateTime(rtcz.getEpoch());
    snprintf(datestring, DATE_STRING_SIZE, "%04d-%02d-%02d", dt.year(), dt.month(), dt.day());
    String date = String(datestring);
    String filename = "datalog_" + date + ".txt";
    DateTime aged_dt = DateTime(rtcz.getEpoch() - MAX_LOGFILE_AGE);
    snprintf(datestring, DATE_STRING_SIZE, "%04d-%02d-%02d", aged_dt.year(), aged_dt.month(), aged_dt.day());
    String aged_date = String(datestring);
    String aged_filename = "datalog_" + aged_date + ".txt";
    if (sd.exists(aged_filename.c_str())) {
        sd.remove(aged_filename.c_str());
    }
    File file;
    logln(F("Writing log lines to filename"));
    file = sd.open(filename, O_WRITE|O_APPEND|O_CREAT);  // will create file if it doesn't exist
    while (cursor != NULL) {
        logln(cursor->data);
        file.println(cursor->data);
        if (!DO_TRANSMIT_DATA) {
            DataSample *_cursor = cursor;
            cursor = cursor->next;
            datasample_head = cursor;
            free(_cursor);
        } else {
            cursor = cursor->next;
        }
    }
    logln(F("-------"));
    file.close();
    logln(F("File closed"));


    long alarmtime = rtcz.getEpoch() + getNextTaskTEMP() - 3;
    DateTime alarm = DateTime(alarmtime);
    setInterruptTimeoutSched(alarm); 
    standbySched();
}

void _do_transmit(uint8_t to_node)
{
    bool clear = false;
    memset(msg_buf, 0, KL_ROUTER_MAX_MESSAGE_LEN);
    memcpy(msg_buf, "testing", sizeof("testing"));
    msg_buf[0] = '[';
    int data_index = 1;
    logln(F("TRANSMITTING DATA: ------"));
    DataSample *cursor = datasample_head;
    while (cursor != NULL) {
        Watchdog.reset();
        Serial.println("**** Checking strlen of data");
        //logln(cursor->data);
        Serial.println(cursor->data);
        Serial.println(strlen(cursor->data));
        Serial.println("***********************");
        if (data_index + strlen(cursor->data) >= KL_ROUTER_MAX_MESSAGE_LEN) {
            logln(F("Sending partial data history: "));
            Serial.println((char*)msg_buf);
            msg_buf[data_index-1] = ']';
            // TODO: send a have-more-data flag until end
            sendLoRaMessage(msg_buf, data_index, to_node);
            delay(1000);
            //delay(5000);  // TODO(Anyone): better handling on the collector side?
            memset(msg_buf, 0, KL_ROUTER_MAX_MESSAGE_LEN);
            msg_buf[0] = '[';
            data_index = 1;
        }
        memcpy(&msg_buf[data_index], cursor->data, strlen(cursor->data));
        data_index += strlen(cursor->data);
        msg_buf[data_index++] = ',';
        if (clear) {
            DataSample *_cursor = cursor;
            cursor = cursor->next;
            datasample_head = cursor;
            free(_cursor);
        } else {
            cursor = cursor->next;
        }
    }
    logln(F("-------"));
    if (data_index > 1) {
        msg_buf[data_index-1] = ']';
        logln(F("Sending message remainder"));
        Serial.println((char*)msg_buf);
        Serial.print("Length: ");
        Serial.println(data_index);
        //Watchdog.reset();
        //sendLoRaMessage(msg_buf, data_index, config.collector_id);
        /*
        if (LoRaRouter->sendtoWait(msg_buf, data_index, 10) == RH_ROUTER_ERROR_NONE) {
            Serial.println("Sent data");
        } else {
            Serial.println("Error sending data");
        }
        */
        delay(500);
        //delay(5000);
    }
    //LoRaRadio->sleep();
}

void transmitData() {
    Watchdog.enable();

    //sendLoRaMessage(msg_buf, sizeof("testing"), config.collector_id);
    /*
    if (LoRaRouter->sendtoWait(msg_buf, sizeof("testing"), 10) == RH_ROUTER_ERROR_NONE) {
        Serial.println("Sent message");
    } else {
        Serial.println("Error sending message");
    }
    */
    uint8_t len = sizeof(msg_buf);
    uint8_t source;
    uint8_t dest;
    uint8_t flags;
    //Serial.println("Setting up LoRa");
    //setupLoRa(2, 8, 3, 10);
    Serial.println("Waiting for request to send data.");
    // Everyone will send an arp within the first 2 seconds of the collection time in order to
    // attach any stray nodes.
    unsigned long arp_time = millis() + random(0, 2000);
    bool did_arp = false;
    while (1) {
        // TODO: we want to time-limit this, perhaps based on the time of the next scheduled event?
        Watchdog.reset();
        if (LoRaRouter->recvfromAck(msg_buf, &len, &source, &dest, &flags)) {
            Serial.print("Received message from: ");
            Serial.println(source);
            Serial.println((char*)msg_buf);
            // TODO: check to see if this is a data request. For now, assume that it is
            _do_transmit(source);
            break;
        } else if (!did_arp && millis() > arp_time) {
            Serial.println("Sending randomized arp to find stray nodes.");
            LoRaRouter->doArp(0);
            did_arp = true;
            LoRaRouter->printRoutingTable();
        }
    }
}


void readDataSamples() {
    Watchdog.reset();
    SensorConfig *cursor = sensor_config_head;
    while (cursor) {
        log_(F("Reading sensor %s .. "), cursor->sensor->id);
        DataSample *sample = appendData();
        cursor->sensor->read(sample->data, DATASAMPLE_DATASIZE);
        logln("Stopping sensor");
        cursor->sensor->stop();
        logln(F("Sensor stopped: %s"), cursor->sensor->id);
        cursor = cursor->next;
    }
    digitalWrite(12, LOW);
    digitalWrite(LED_BUILTIN, LOW);

    long alarmtime = rtcz.getEpoch() + getNextTaskTEMP() - 3;
      
    DateTime dt = DateTime(alarmtime);
    setInterruptTimeoutSched(dt); 
    standbySched();
}

void flashHeartbeat() {
    Watchdog.enable();
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);    // turn the LED off
    delay(100);
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);    // turn the LED off
}

void sendBeacon() {
    Watchdog.enable();
    Serial.println("Sending hello beacon to network");
    LoRaRouter->doArp(0);
    LoRaRouter->printRoutingTable();
    return;

    if (LoRaRouter->routingTableIsEmpty()) {
        digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on
        Serial.println("Sending hello beacon to network");
        LoRaRouter->doArp(0);
        LoRaRouter->printRoutingTable();
        digitalWrite(LED_BUILTIN, LOW);    // turn the LED off
    } else {
        Serial.println("Have routes. Not sending beacon.");
        LoRaRouter->printRoutingTable();
    }
}

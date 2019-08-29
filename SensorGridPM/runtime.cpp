/**
 * Copyright 2018 Northwestern University
 */
#include "config.h"
#include "runtime.h"
#include <ArduinoJson.h>
#include <avr/dtostrf.h>
#include <console.h>
#include <LoRa.h>
#include <DataManager.h>
#include <LoRaHarvest.h>
#include <TaskScheduler.h>

#define DATE_STRING_SIZE 11

static uint8_t msg_buf[140] = {0};
static Message *msg = reinterpret_cast<Message*>(msg_buf);
StaticJsonBuffer<200> json_buffer;
JsonArray& data_array = json_buffer.createArray();

Task initialize(60, TASK_FOREVER, &initSensors);
Task sample(60, TASK_FOREVER, &readDataSamples);
//Task _log(180, TASK_FOREVER, &logData);
Task heartbeatOn(5, TASK_FOREVER, &flashHeartbeatOn);
Task heartbeatOff(5, TASK_FOREVER, &flashHeartbeatOff);
Scheduler runner;


DataSample *datasample_head = NULL;
DataSample *datasample_tail = NULL;


void syncTime(uint32_t timestamp) {
    uint32_t uptime = rtcz.getEpoch() - systemStartTime();
    runner.disableAll();
    rtcz.setEpoch(timestamp);
    systemStartTime(timestamp - uptime);
    runner.enableAll();
}

long nextIterationTime(Task &aTask)
{
    return runner.timeUntilNextIteration(aTask);
}

long getNextTaskTEMP() {
  long minTime = nextIterationTime(heartbeatOn);
  if (nextIterationTime(heartbeatOff) < minTime) {
      minTime = nextIterationTime(heartbeatOff);
  }
  if (nextIterationTime(initialize) < minTime) {
      minTime = nextIterationTime(initialize);
  }
  //if (runner.timeUntilNextIteration(initialize) < minTime) {
  //  minTime = runner.timeUntilNextIteration(initialize);
  //}
  if (nextIterationTime(sample) < minTime) {
      minTime = nextIterationTime(sample);
  }
  //if (runner.timeUntilNextIteration(sample) < minTime) {
  //  minTime = runner.timeUntilNextIteration(sample);
  //}
  //if (runner.timeUntilNextIteration(_log) < minTime) {
  //  minTime = runner.timeUntilNextIteration(_log);
  //}
  //if (runner.timeUntilNextIteration(heartbeat) < minTime) {
  //  minTime = runner.timeUntilNextIteration(heartbeat);
  //}
  //if (nextIterationTime(heartbeatOn) < minTime) {
  //    minTime = nextIterationTime(heartbeatOn);
  //}
  //if (runner.timeUntilNextIteration(heartbeatOn) < minTime) {
  //  minTime = runner.timeUntilNextIteration(heartbeatOn);
  //}
  //if (runner.timeUntilNextIteration(heartbeatOff) < minTime) {
  //  minTime = runner.timeUntilNextIteration(heartbeatOff);
  //}
  return minTime;
}

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

/*
uint32_t getNextCollectionTime() {
    int period = config.collection_period;
    uint32_t t = rtcz.getEpoch();
    int d = t % period;
    return (t + period - d);
}
*/

/*
static void standby() {
    mode = STANDBY;
    if (DO_STANDBY) {
        Watchdog.disable();
        rtcz.standbyMode();
    }
}
*/

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
    static bool standby_mode = true;
    if (DO_STANDBY) {
      //println("standbyMode: %d", standby_mode);
      if (standby_mode) {
          Watchdog.disable();
          rtcz.standbyMode();
      }
    } 
}

/* end local utils */

/* runtime mode handlers */

void initSensors() {
    Watchdog.enable();
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
/*
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
        // When we first hit the threshold, stop all the sensors

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
*/

void recordBatteryLevel() {
    // float bat = batteryLevel();
    println("Recording battery ..");
    char bat[5];
    ftoa(batteryLevelAveraged(), bat, 2);
    DataSample *batSample = appendData();
    snprintf(batSample->data, DATASAMPLE_DATASIZE, "{\"node\":%d,\"bat\":%s,\"ts\":%lu}",
        config.node_id, bat, rtcz.getEpoch());
    recordData(batSample->data, strlen(batSample->data));
    println(".. done recording battery");
}



void recordUptime() {
    println("recording uptime ..");
    uint32_t runtime = millis() / 1000;
    uint32_t uptime = rtcz.getEpoch() - systemStartTime();
    Serial.println(rtcz.getEpoch());
    Serial.println(systemStartTime());
    Serial.println(uptime);
    DataSample *sample = appendData();
    snprintf(sample->data, DATASAMPLE_DATASIZE, "{\"node\":%d,\"uptime\":%lu,\"runtime\":%lu,\"ts\":%lu}",
        config.node_id, uptime, runtime, rtcz.getEpoch());
    recordData(sample->data, strlen(sample->data));
    println(".. done recording uptime");
}

void logData() {
    Watchdog.enable();
    recordBatteryLevel();                   // Should get a battery recording level before each log
    recordUptime();
    Watchdog.enable();                    // Enabled watchdog
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
    long delay = getNextTaskTEMP();
    if (delay > 2) {
        long alarmtime = rtcz.getEpoch() + delay;
        DateTime alarm = DateTime(alarmtime);
        setInterruptTimeoutSched(alarm); 
        standbySched();
    }
}

void readDataSamples() {
    Watchdog.enable();
    SensorConfig *cursor = sensor_config_head;
    while (cursor) {
        log_(F("Reading sensor %s .. "), cursor->sensor->id);
        DataSample *sample = appendData();
        size_t len = cursor->sensor->read(sample->data, DATASAMPLE_DATASIZE);
        recordData(sample->data, len);
        cursor->sensor->stop();
        logln(F("Sensor stopped: %s"), cursor->sensor->id);
        cursor = cursor->next;
    }
    recordBatteryLevel();
    recordUptime();
    digitalWrite(12, LOW);
    digitalWrite(LED_BUILTIN, LOW);
    long delay = getNextTaskTEMP();
    if (delay > 2) {
        long alarmtime = rtcz.getEpoch() + delay;
        DateTime alarm = DateTime(alarmtime);
        print("setInterrupTimeoutSched delay: %d", delay);
        setInterruptTimeoutSched(alarm); 
        standbySched();
    }
}

void flashHeartbeat() {
    Watchdog.enable();
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on
    print(".");
    delay(200);
    digitalWrite(LED_BUILTIN, LOW);    // turn the LED off
    print(".");
    long delay = getNextTaskTEMP();
    if (delay > 2) {
        long alarmtime = rtcz.getEpoch() + delay;
        DateTime alarm = DateTime(alarmtime);
        setInterruptTimeoutSched(alarm);
        standbySched();
    } else {
        print("Delay is only: %d", delay);
    }
}

void flashHeartbeatOn() {
    Watchdog.enable();
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on
    print(".");
    //long delay = getNextTaskTEMP();
    //if (delay > 2) {
    //    long alarmtime = rtcz.getEpoch() + delay;
    //    DateTime alarm = DateTime(alarmtime);
    //    setInterruptTimeoutSched(alarm);
    //    standbySched();
    //}
}

void flashHeartbeatOff() {
    Watchdog.enable();
    digitalWrite(LED_BUILTIN, LOW);   // turn the LED on
    long delay = getNextTaskTEMP();
    if (delay > 2) {
        println("current time: %d", rtcz.getEpoch());
        long alarmtime = rtcz.getEpoch() + delay;
        println("alarm time: %d", alarmtime);
        DateTime alarm = DateTime(alarmtime);
        print("setInterrupTimeoutSched delay: %d", delay);
        setInterruptTimeoutSched(alarm);
        standbySched();
    }
}

void setupRunner()
{
    /*
    Serial.print("Start time: ");
    Serial.println(_TASK_TIME_FUNCTION());
    runner.init();
    Serial.println("Initialized scheduler");
    runner.addTask(initialize);
    Serial.println("added initialize");
    runner.addTask(sample);
    Serial.println("added sample");
    //runner.addTask(_log);
    //Serial.println("added log");
    runner.addTask(heartbeatOn);
    runner.addTask(heartbeatOff);
    Serial.println("added heartbeat");
    int wait_time = 0;
    if (_TASK_TIME_FUNCTION() % 60 != 0){ // Will wait to start initialization on the minute
      wait_time = 60 - (_TASK_TIME_FUNCTION() % 60);
    }
    Serial.print("Wait time: ");
    Serial.println(wait_time);
    initialize.enableDelayed(wait_time);
    Serial.println("Enabled initialize");
    sample.enableDelayed(7 + wait_time);
    Serial.println("Enabled sample with 7 second delay");
    */
    runner.init();
    //addRunnerTask(heartbeatOn);
    //addRunnerTask(heartbeatOff);
    //addRunnerTask(initialize);
    //addRunnerTask(sample);
    runner.addTask(heartbeatOn);
    runner.addTask(heartbeatOff);
    runner.addTask(initialize);
    runner.addTask(sample);
    heartbeatOn.enable();
    heartbeatOff.enableDelayed(1);
    initialize.enable();
    sample.enableDelayed(7);
    setTimeSyncFcn(syncTime);
}

void runRunner()
{
    runner.execute();
}
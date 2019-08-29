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
#include "tests.h"
//#include <TaskScheduler.h>
#include <LoRa.h>
#include <LoRaHarvest.h>

#define SET_CLOCK false

WatchdogType Watchdog;

//enum Mode mode = WAIT;

//RTCZero rtcz;
RTC_PCF8523 rtc;
int start_epoch;
OLED oled = OLED(rtc);

// uint32_t sampleRate = 10; // sample rate of the sine wave in Hertz, how many times per second the TC5_Handler() function gets called per second basically
// int MAX_TIMEOUT = 10;
// unsigned long sample_period = 60 * 10;
// unsigned long heartbeat_period = 30;
// unsigned long next_sample;
// int sensor_init_time = 7;

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

/* moved to config.cpp
uint32_t getTime() {
    return rtcz.getEpoch();
}
*/

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
    loadSensorConfig(config.node_id, getTime);
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

// TASK STUFF
//Task& getNextTask();

//void initializeCallback();
//void sampleCallback();
//void logCallback();

// Need to change Callbacks to actual functions
/*
Task initialize(60, TASK_FOREVER, &initSensors);
Task sample(60, TASK_FOREVER, &readDataSamples);
//Task _log(180, TASK_FOREVER, &logData);
Task heartbeatOn(5, TASK_FOREVER, &flashHeartbeatOn);
Task heartbeatOff(5, TASK_FOREVER, &flashHeartbeatOff);
Scheduler runner;
*/

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

/*
long getNextTaskTEMP() {
  long minTime = 500;
  if (runner.timeUntilNextIteration(initialize) < minTime) {
    minTime = runner.timeUntilNextIteration(initialize);
  }
  if (runner.timeUntilNextIteration(sample) < minTime) {
    minTime = runner.timeUntilNextIteration(sample);
  }
  //if (runner.timeUntilNextIteration(_log) < minTime) {
  //  minTime = runner.timeUntilNextIteration(_log);
  //}
  //if (runner.timeUntilNextIteration(heartbeat) < minTime) {
  //  minTime = runner.timeUntilNextIteration(heartbeat);
  //}
  if (runner.timeUntilNextIteration(heartbeatOn) < minTime) {
    minTime = runner.timeUntilNextIteration(heartbeatOn);
  }
  if (runner.timeUntilNextIteration(heartbeatOff) < minTime) {
    minTime = runner.timeUntilNextIteration(heartbeatOff);
  }
  return minTime;
}
*/

/*
void initializeCallback() {
//  oled.clear();
//  oled.setCursor(0,0);
  Serial.print("Initializing ");
  Serial.println(_TASK_TIME_FUNCTION());
//  oled.print("Init: ");
//  oled.println(_TASK_TIME_FUNCTION());
//  oled.print("Next task: ");
//  oled.println(getNextTaskTEMP());
//  oled.display();
  Serial.print("The time to the next task is: ");
  Serial.println(getNextTaskTEMP());
}
*/

/*
void sampleCallback() {
//  oled.clear();
//  oled.setCursor(0,0);
  Serial.print("Sampling ");
  Serial.println(_TASK_TIME_FUNCTION());
  Serial.print("The time to the next task is: ");
  Serial.println(getNextTaskTEMP());
  long alarmtime = _TASK_TIME_FUNCTION() + getNextTaskTEMP() - 3;
  Serial.print("We are setting the alarm to ");
  Serial.println(alarmtime);
//  oled.print("Sampling: ");
//  oled.println(_TASK_TIME_FUNCTION());
//  oled.print("Next task: ");
//  oled.println(getNextTaskTEMP());
//  oled.print("Alarm: ");
//  oled.println(alarmtime);
//  oled.display();
  DateTime dt = DateTime(alarmtime);
  setInterruptTimeoutTEMP(dt); 
  standbyTEMP(); 
}
*/

/*
void logCallback() {
//  oled.clear();
//  oled.setCursor(0,0);
  Serial.print("Logging ");
  Serial.println(_TASK_TIME_FUNCTION());
  Serial.print("The time to the next task is: ");
  Serial.println(getNextTaskTEMP()); 
  long alarmtime = _TASK_TIME_FUNCTION() + getNextTaskTEMP() - 3;
  Serial.print("We are setting the alarm to ");
  Serial.println(alarmtime);
//  oled.print("Logging: ");
//  oled.println(_TASK_TIME_FUNCTION());
//  oled.print("Next task: ");
//  oled.println(getNextTaskTEMP());
//  oled.print("Alarm: ");
//  oled.println(alarmtime);
//  oled.display();
  DateTime dt = DateTime(alarmtime);
  setInterruptTimeoutTEMP(dt);
  standbyTEMP();
}
*/

/*
int custom_handlePacket(int to, int from, int dest, int seq, int packetType, uint32_t timestamp, uint8_t *route, size_t route_size, uint8_t *message, size_t msg_size, int topology)
{
    static int last_seq = 0;
    if (seq == last_seq) {
        return MESSAGE_CODE_DUPLICATE_SEQUENCE;
    }
    if (to != nodeId() && to != 255) return MESSAGE_CODE_WRONG_ADDRESS;
    if (!topologyTest(topology, to, from)) return MESSAGE_CODE_TOPOLOGY_FAIL;
    last_seq = seq;
    uint8_t packet_id;
    if (dest == nodeId()) {
        switch (packetType) {
            case PACKET_TYPE_SENDDATA:
                packet_id = message[0];
                rtcz.setEpoch(timestamp);
                sendDataPacket(packet_id, ++last_seq, route, route_size); // TODO: get packet # request from message
                return MESSAGE_CODE_SENT_NEXT_DATA_PACKET;
            case PACKET_TYPE_DATA:
                handleDataMessage(route[0], message, msg_size);
                return MESSAGE_CODE_RECEIVED_DATA_PACKET;
            case PACKET_TYPE_STANDBY:
                standby(message[0]); // up to 255 seconds. TODO, use 2 bytes for longer timeouts
                return MESSAGE_CODE_STANDBY;
            case PACKET_TYPE_ECHO:
                if (!isCollector) {
                    handleEchoMessage(++last_seq, route, route_size, message, msg_size);
                } else {
                    println("MESSAGE:");
                    for (uint8_t i=0; i<msg_size; i++) {
                        print("%d ", message[i]);
                    }
                    println("");
                }
        }
    } else if (dest == 255) { // broadcast message
        switch (packetType) {
            case PACKET_TYPE_STANDBY:
                if (!isCollector) {
                    println("REC'd BROADCAST STANDBY FOR: %d", message[0]);
                    routeMessage(255, last_seq, PACKET_TYPE_STANDBY, route, route_size, message, msg_size);
                    standby(message[0]);
                }
                return MESSAGE_CODE_STANDBY;
        }
    } else { // route this message
        routeMessage(dest, last_seq, packetType, route, route_size, message, msg_size);
        return MESSAGE_CODE_ROUTED;
    }
    return MESSAGE_CODE_NONE;
}
*/

/*
void custom_onReceive(int packetSize)
{
    static uint8_t route_buffer[MAX_ROUTE_SIZE];
    static uint8_t msg_buffer[255];
    int to = LoRa.read();
    int from = LoRa.read();
    int dest = LoRa.read();
    int seq = LoRa.read();
    int type = LoRa.read();
    uint32_t ts = LoRa.read() << 24 | LoRa.read() << 16 | LoRa.read() << 8 | LoRa.read();
    size_t route_idx_ = 0;
    size_t msg_idx_ = 0;
    print("REC'D: TO: %d; FROM: %d; DEST: %d; SEQ: %d; TYPE: %d; RSSI: %d; ts: %u",
        to, from, dest, seq, type, LoRa.packetRssi(), ts);
    print("; ROUTE:");
    while (LoRa.available()) {
        uint8_t node = LoRa.read();
        if (node == 0) break;
        route_buffer[route_idx_++] = node;
        print(" %d", route_buffer[route_idx_-1]);
    }
    println("");
    println("SNR: %f; FRQERR: %f", LoRa.packetSnr(), LoRa.packetFrequencyError());
    while (LoRa.available()) {
        msg_buffer[msg_idx_++] = LoRa.read();
    }
    custom_handlePacket(to, from, dest, seq, type, ts, route_buffer, route_idx_, msg_buffer, msg_idx_, 0);
}
*/

/*
void custom_setupLoRa(int csPin, int resetPin, int irqPin)
{
    LoRa.setPins(csPin, resetPin, irqPin);
    if (!LoRa.begin(frequency)) {
        println("LoRa init failed.");
        while(true);
    }
    LoRa.enableCrc();
    LoRa.onReceive(custom_onReceive);
    LoRa.receive();
}
*/

void setup() {
    rtcz.begin();
    systemStartTime(rtcz.getEpoch());
    Watchdog.enable();
    unsigned long _start = millis();
    while ( !Serial && (millis() - _start) < WAIT_SERIAL_TIMEOUT ) {}
    config.loadConfig();
    nodeId(config.node_id);
    setupLoRa(config.RFM95_CS, config.RFM95_RST, config.RFM95_INT);

    /*
    runner.init();
    runner.addTask(heartbeatOn);
    runner.addTask(heartbeatOff);
    runner.addTask(initialize);
    runner.addTask(sample);
    heartbeatOn.enable();
    heartbeatOff.enableDelayed(1);
    initialize.enable();
    sample.enableDelayed(7);
    */
    setupRunner();
    Watchdog.disable();
}

void loop() {
    Watchdog.enable();
    runRunner();
    //runner.execute();
//    if (oled.isOn()) {
//        updateClock();
//        oled.displayDateTime();
//    }
    // How often should we check battery level?
//    if (!checkBatteryLevel()) {
//        return;
//    }

    // enable (instead of reset) Watchdog every loop because we disable during standby
    // Watchdog.enable();

    //static uint32_t start_time = getTime();
//    static uint32_t uptime;
//    uptime = millis();

    /**
     * Some apparent lockups may actually be indefinite looping on STANDBY status
     * if for some reason the scheduled interrupt never happens to kick us out of
     * STANDBY mode. For that reason, the standby_timer will track time and should
     * never get to be longer than the scheduled heartbeat period.
     */
    //static uint32_t standby_timer = getTime();
}

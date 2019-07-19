/**
 * Knight Lab SensorGrid
 *
 * Wireless air pollution (Particulate Matter PM2.5 & PM10) monitoring over LoRa radio
 * 
 * Copyright 2018 Northwestern University
 */
#include <KnightLab_GPS.h>
#include "config.h"
#include "rh_lora.h"
#include "runtime.h"
#include "tests.h"
#include <TaskScheduler.h>
#include <LoRa.h>
#include <LoRaHarvest.h>

#define SET_CLOCK false
#define NOTEST




WatchdogType Watchdog;

enum Mode mode = WAIT;

RTCZero rtcz;
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
      updateClock();
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
Task& getNextTask();

void initializeCallback();
void sampleCallback();
void logCallback();

// Need to change Callbacks to actual functions
Task initialize(60, TASK_FOREVER, &initSensors);
Task sample(60, TASK_FOREVER, &readDataSamples);
Task _log(180, TASK_FOREVER, &logData);
Task heartbeat(5, TASK_FOREVER, &flashHeartbeat);

Scheduler runner;

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

long getNextTaskTEMP() {
  long minTime = 500;
  
  if (runner.timeUntilNextIteration(initialize) < minTime) {
    minTime = runner.timeUntilNextIteration(initialize);
  }
  if (runner.timeUntilNextIteration(sample) < minTime) {
    minTime = runner.timeUntilNextIteration(sample);
  }
  if (runner.timeUntilNextIteration(_log) < minTime) {
    minTime = runner.timeUntilNextIteration(_log);
  }
  if (runner.timeUntilNextIteration(heartbeat) < minTime) {
    minTime = runner.timeUntilNextIteration(heartbeat);
  }

  return minTime;
}

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


/* setup and loop */
void setup() {
    Watchdog.enable();
    /* This is causing lock-up. Need to do further research into low power modes
       on the Cortex M0 */
    // oled.setButtonFunction(BUTTON_A, *aButton_ISR, CHANGE);
    // oled.displayDateTime();
    unsigned long _start = millis();
    while ( !Serial && (millis() - _start) < WAIT_SERIAL_TIMEOUT ) {}

    #ifdef TEST
    aunit::TestRunner::setVerbosity(aunit::Verbosity::kAll);
    return;
    #endif
    
//    setupGPS();
//    setupClocks();
    oled.init();
    oled.displayStartup();
    if (ALWAYS_LOG || Serial) {
        setupLogging();
    }


    // Clock set up
    static volatile int buttonHeld = !digitalRead(BUTTON_A);
    bool state = false;
    if (buttonHeld != 0) {
      state = true;
    }
    if (state) {
      while(1) {
        Watchdog.reset();
        delay(2000);
        Serial.println("Just waiting for the time now...");
        if (Serial.find("t")) {
          start_epoch = Serial.parseInt();
          setRTC();
          break;
        }
      }
    }

    setupGPS();
    setupClocks();
    setStartTime();
    
    
    logln(F("begin setup .."));
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    config.loadConfig();
    //logln("Setting up LoRa radio");
    //setupRadio(config.RFM95_CS, config.RFM95_INT, config.node_id);
    //radio->sleep();
    setupSensors();
    // setupHoneywell();
    // ADAFRUIT_SI7021::setup(config.node_id, &getTime);
    // This is done in RTCZero::standbyMode
    // https://github.com/arduino-libraries/RTCZero/blob/master/src/RTCZero.cpp
    // SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);
    logln(F(".. setup complete"));
    printCurrentTime();
    oled.endDisplayStartup();

    // Task Setup
    Serial.print("Start time: ");
    Serial.println(_TASK_TIME_FUNCTION());

    runner.init();
    Serial.println("Initialized scheduler");

    runner.addTask(initialize);
    Serial.println("added initialize");
    runner.addTask(sample);
    Serial.println("added sample");
    runner.addTask(_log);
    Serial.println("added log");
    runner.addTask(heartbeat);
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
    _log.enableDelayed(207 + wait_time); // 207 seconds to start after 3 minutes and then 20 seconds after sample
    Serial.println("Enabled log with 20 second delay to occur every 3 minutes");
    heartbeat.enableDelayed(wait_time);
    Watchdog.disable();

    nodeId(config.node_id);
    if (config.node_id == 1) isCollector = true;
    setupLoRa(config.RFM95_CS, config.RFM95_RST, config.RFM95_INT);
}

void loop() {
    #ifdef TEST
    aunit::TestRunner::run();
    return;
    #endif

    runner.execute();
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

    static uint32_t start_time = getTime();
//    static uint32_t uptime;
//    uptime = millis();

    /**
     * Some apparent lockups may actually be indefinite looping on STANDBY status
     * if for some reason the scheduled interrupt never happens to kick us out of
     * STANDBY mode. For that reason, the standby_timer will track time and should
     * never get to be longer than the scheduled heartbeat period.
     */
    static uint32_t standby_timer = getTime();
//    if (mode == STANDBY) {
//        if (getTime() - standby_timer > config.heartbeat_period) {
//            standby_timer = getTime();
//            mode = WAIT;
//        }
//        return;
//    }
//
//    if (!checkBatteryLevel()) {
//        return;
//    }
//
//    static uint32_t next_collection_time = getNextCollectionTime();
//    if (start_time && getTime() - start_time > 3 * 60) {
//        oled.off();
//    }
//    if (oled.isOn()) {
//        updateClock();
//        oled.displayDateTime();
//    }
//    if (mode == WAIT) {
//        setInitTimeout();
//    } else if (mode == INIT) {
//        initSensors();
//        setSampleTimeout();
//    } else if (mode == SAMPLE) {
//        readDataSamples();
//        if (getTime() > next_collection_time) {
//            setCommunicateDataTimeout();
//        } else {
//            mode = WAIT;
//        }
//    } else if (mode == COMMUNICATE) {
//        recordBatteryLevel();
//        recordUptime(uptime);
//        if (DO_LOG_DATA) {
//            logData(!DO_TRANSMIT_DATA);
//        }
//        if (DO_TRANSMIT_DATA) {
//            transmitData(true);
//        }
//        next_collection_time = getNextCollectionTime();
//        mode = WAIT;
//    } else if (mode == HEARTBEAT) {
//        flashHeartbeat();
//        mode = WAIT;
//    }
}

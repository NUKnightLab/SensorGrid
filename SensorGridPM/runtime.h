/**
 * Copyright 2018 Northwestern University
 */
#ifndef SENSORGRIDPM_RUNTIME_H_
#define SENSORGRIDPM_RUNTIME_H_

// #include <Adafruit_SleepyDog.h>
#include <SdFat.h>

static SdFat SD;


//extern void setInitTimeout();
// extern void initSensors();
//extern void setSampleTimeout();
extern void recordDataSamples();
//extern void setCommunicateDataTimeout();
//extern void flashHeartbeat();
extern void flashHeartbeatOn();
extern void flashHeartbeatOff();
extern void recordBatteryLevel();
extern void recordRestart();
//extern void recordTempAndHumidity();
extern void recordUptime();
// extern void logData(bool clear);
//extern void transmitData(bool clear);
//extern uint32_t getNextCollectionTime();
// extern void readDataSamples();
extern bool checkBatteryLevel();
void setupRunner();
void runRunner();

/* data history */
struct DataSample {
    char data[DATASAMPLE_DATASIZE];
    struct DataSample *next;
};

#endif  // SENSORGRIDPM_RUNTIME_H_

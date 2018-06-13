/**
 * Copyright 2018 Northwestern University
 */
#ifndef KNIGHTLAB_SENSORGRID_SENSORGRIDPM_RUNTIME_H_
#define KNIGHTLAB_SENSORGRID_SENSORGRIDPM_RUNTIME_H_

#include <SdFat.h>
static SdFat SD;


extern void setInitTimeout();
extern void initSensors();
extern void setSampleTimeout();
extern void recordDataSamples();
extern void setCommunicateDataTimeout();
extern void flashHeartbeat();
extern void recordBatteryLevel();
extern void logData(bool clear);
extern void transmitData(bool clear);
extern uint32_t getNextCollectionTime();
static uint32_t getNextPeriodTime(int period);

#endif  // KNIGHTLAB_SENSORGRID_SENSORGRIDPM_RUNTIME_H_

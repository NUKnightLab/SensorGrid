/**
 * Copyright 2018 Northwestern University
 */
#ifndef KNIGHTLAB_SENSORGRID_SENSORGRIDPM_RUNTIME_H_
#define KNIGHTLAB_SENSORGRID_SENSORGRIDPM_RUNTIME_H_

#include <SdFat.h>
static SdFat SD;


extern void set_init_timeout();
extern void init_sensors();
extern void set_sample_timeout();
extern void record_data_samples();
extern void setCommunicateDataTimeout();
extern void communicate_data();
extern void flash_heartbeat();
extern void logData(bool clear);
extern uint32_t getNextCollectionTime();

#endif  // KNIGHTLAB_SENSORGRID_SENSORGRIDPM_RUNTIME_H_

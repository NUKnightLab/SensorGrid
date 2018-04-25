#ifndef __SENSORGRID_RUNTIME__
#define __SENSORGRID_RUNTIME__

#include <SdFat.h>
static SdFat SD;

extern void set_init_timeout();
extern void init_sensors();
extern void set_sample_timeout();
extern void record_data_samples();
extern void set_communicate_data_timeout();
extern void communicate_data();
extern void flash_heartbeat();

#endif

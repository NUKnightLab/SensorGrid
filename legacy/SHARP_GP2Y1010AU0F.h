#ifndef SHARP_GP2Y1010AU0F_H
#define SHARP_GP2Y1010AU0F_H

#include <Arduino.h>

#define DUST_SENSOR_LED_POWER 12
#define DUST_SENSOR_SAMPLING_TIME 280
#define DUST_SENSOR_DELTA_TIME 40
#define DUST_SENSOR_SLEEP_TIME 9680
#define DUST_SENSOR_VCC 3.3
#define DUST_SENSOR_LED_ON LOW
#define DUST_SENSOR_LED_OFF HIGH

namespace SHARP_GP2Y1010AU0F {
    void setDataPin(uint8_t pin);
    bool setup(uint8_t data_pin);
    float read_legacy();
    int32_t read();
    int32_t read_average(uint8_t num_samples);
}

#endif

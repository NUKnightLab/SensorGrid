#ifndef GROVE_AIR_QUALITY_H
#define GROVE_AIR_QUALITY_H

#include <Arduino.h>

namespace GROVE_AIR_QUALITY_1_3 {
    void setup(uint8_t data_pin);
    float read();
}

#endif

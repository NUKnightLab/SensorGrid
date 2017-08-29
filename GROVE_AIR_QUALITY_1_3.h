#ifndef GROVE_AIR_QUALITY_H
#define GROVE_AIR_QUALITY_H

#include <Arduino.h>

namespace GROVE_AIR_QUALITY_1_3 {
    bool setup(uint8_t data_pin);
    float read_legacy();
    int32_t read();
}

#endif

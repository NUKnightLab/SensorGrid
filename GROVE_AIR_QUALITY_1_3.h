#ifndef GROVE_AIR_QUALITY_H
#define GROVE_AIR_QUALITY_H

#include "SensorGrid.h"

namespace GROVE_AIR_QUALITY_1_3 {
    void setDataPin(uint8_t);
    void setup();
    float read();
}
#endif

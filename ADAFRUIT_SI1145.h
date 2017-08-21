#ifndef ADAFRUIT_SI1145_H
#define ADAFRUIT_SI1145_H

#include "SensorGrid.h"
#include <Adafruit_SI1145.h>

namespace ADAFRUIT_SI1145 {
    void setup();
    float readVisible();
    float readIR();
    float readUV();
}

#endif

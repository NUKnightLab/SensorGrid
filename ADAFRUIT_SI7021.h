#ifndef ADAFRUIT_SI7021_H
#define ADAFRUIT_SI7021_H

#include "SensorGrid.h"
#include <Adafruit_Si7021.h>

namespace ADAFRUIT_SI7021 {
    void setup();
    float readTemperature();
    float readHumidity();
}

#endif

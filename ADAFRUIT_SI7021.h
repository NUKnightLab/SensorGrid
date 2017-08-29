#ifndef ADAFRUIT_SI7021_H
#define ADAFRUIT_SI7021_H

#include <Adafruit_Si7021.h>

namespace ADAFRUIT_SI7021 {
    bool setup();
    int32_t readTemperature();
    int32_t readHumidity();
}

#endif

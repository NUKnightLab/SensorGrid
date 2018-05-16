#ifndef ADAFRUIT_SI1145_H
#define ADAFRUIT_SI1145_H

#include <Adafruit_SI1145.h>

namespace ADAFRUIT_SI1145 {
    bool setup();
    int32_t readVisible();
    int32_t readIR();
    int32_t readUV();
}

#endif

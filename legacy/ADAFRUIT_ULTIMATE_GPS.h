#ifndef ADAFRUIT_ULTIMATE_GPS_H
#define ADAFRUIT_ULTIMATE_GPS_H

#include <KnightLab_GPS.h>

namespace ADAFRUIT_ULTIMATE_GPS {
    bool setup();
    int32_t fix();
    int32_t satellites();
    int32_t satfix();
    int32_t latitudeDegrees();
    int32_t longitudeDegrees();
}

#endif

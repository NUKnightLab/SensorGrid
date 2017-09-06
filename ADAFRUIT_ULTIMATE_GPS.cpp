#include "ADAFRUIT_ULTIMATE_GPS.h"

/* Adafruit Ultimate GPS FeatherWing */

namespace ADAFRUIT_ULTIMATE_GPS {

    /* GPS is a global */

    bool setup()
    {
        setupGPS();
        return true; // TODO: can we determine if GPS is really setup?
    }

    int32_t fix() {
        return GPS.fix;
    }

    int32_t satellites() {
        return GPS.satellites;
    }

    int32_t satfix() {
        if (GPS.fix) return GPS.satellites;
        return -1 * GPS.satellites;
    }

    int32_t latitudeDegrees() {
        return (int32_t)(roundf(GPS.latitudeDegrees * 1000));
    }

    int32_t longitudeDegrees() {
        return (int32_t)(roundf(GPS.longitudeDegrees * 1000));
    }
}


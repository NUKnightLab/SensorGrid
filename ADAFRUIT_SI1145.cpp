#include "ADAFRUIT_SI1145.h"

/* Adafruit Si1145 IR/UV/Visible light breakout */

namespace ADAFRUIT_SI1145 {

    static Adafruit_SI1145 sensor = Adafruit_SI1145();

    bool setup()
    {
        Serial.print(F("Si1145 "));
        if (sensor.begin()) {
            Serial.println(F("Found"));
            return true;
        } else {
            Serial.println(F("Not Found"));
            return false;
        }
    }

    int32_t readVisible()
    {
        return (int32_t)sensor.readVisible();
    }

    int32_t readIR()
    {
        return (int32_t)sensor.readIR();
    }

    int32_t readUV()
    {
        return (int32_t)(sensor.readUV()*100);
    }
}


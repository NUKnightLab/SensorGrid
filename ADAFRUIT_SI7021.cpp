#include "ADAFRUIT_SI7021.h"

/* Adafruit Si7021 temperature/humidity breakout */

namespace ADAFRUIT_SI7021 {

    static Adafruit_Si7021 sensor = Adafruit_Si7021();

    bool setup()
    {
        Serial.print(F("Si7021 "));
        if (sensor.begin()) {
            Serial.println(F("Found"));
            return true;
        } else {
            Serial.println(F("Not Found"));
            return false;
        }
    }

    int32_t readTemperature()
    {
        return (int32_t)(sensor.readTemperature()*100);
    }

    int32_t readHumidity()
    {
        return (int32_t)(sensor.readHumidity()*100);
    }
}


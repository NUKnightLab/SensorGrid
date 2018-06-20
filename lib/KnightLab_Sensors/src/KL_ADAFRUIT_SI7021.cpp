#include "KL_ADAFRUIT_SI7021.h"

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

    float readTemperature()
    {
        return sensor.readTemperature() * 1.8 + 32;
    }

    float readHumidity()
    {
        return sensor.readHumidity();
    }
}


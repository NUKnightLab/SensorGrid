#include "ADAFRUIT_SI7021.h"

/* Adafruit Si7021 temperature/humidity breakout */

namespace ADAFRUIT_SI7021 {

    static Adafruit_Si7021 sensor = Adafruit_Si7021();

    void setup()
    {
        Serial.print(F("Si7021 "));
        if (sensor.begin()) {
            Serial.println(F("Found"));
        } else {
            Serial.println(F("Not Found"));
        }
    }

    float readTemperature()
    {
        return sensor.readTemperature();
    }

    float readHumidity()
    {
        return sensor.readHumidity();
    }
}


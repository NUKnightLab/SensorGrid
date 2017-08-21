#include "ADAFRUIT_SI1145.h"

/* Adafruit Si1145 IR/UV/Visible light breakout */

namespace ADAFRUIT_SI1145 {

    static Adafruit_SI1145 sensor = Adafruit_SI1145();

    void setup()
    {
        Serial.print(F("Si1145 "));
        if (sensor.begin()) {
            Serial.println(F("Found"));
        } else {
            Serial.println(F("Not Found"));
        }
    }

    float readVisible()
    {
        return sensor.readVisible();
    }

    float readIR()
    {
        return sensor.readIR();
    }

    float readUV()
    {
        return sensor.readUV();
    }
}


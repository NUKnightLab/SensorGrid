#include "GROVE_AIR_QUALITY_1_3.h"

/* TODO: do we need to consider historical averages and slope? See tutorial
 * at: http://wiki.seeed.cc/Grove-Air_Quality_Sensor_v1.3/ and AirQuality
 * library code: https://github.com/SeeedDocument/Grove_Air_Quality_Sensor_v1.3/raw/master/res/AirQuality_Sensor.zip
 */

namespace GROVE_AIR_QUALITY_1_3 {

    static uint8_t data_pin;

    void setDataPin(uint8_t pin) {
        data_pin = pin;
    }

    void setup()
    {
        if (data_pin) {
            Serial.print(F("Setting Grove Air Quality 1.3 pin to: "));
            Serial.println(data_pin, DEC);
            pinMode(data_pin, INPUT);
        }
    }
    
    float read()
    {
        return analogRead(data_pin) * 3.3 / 1024;
    }
}


#include "GROVE_AIR_QUALITY_1_3.h"

/* TODO: do we need to consider historical averages and slope? See tutorial
 * at: http://wiki.seeed.cc/Grove-Air_Quality_Sensor_v1.3/ and AirQuality
 * library code: https://github.com/SeeedDocument/Grove_Air_Quality_Sensor_v1.3/raw/master/res/AirQuality_Sensor.zip
 */

namespace GROVE_AIR_QUALITY_1_3 {

    static uint8_t _data_pin = 0;

    bool setup(uint8_t data_pin)
    {
        if (!data_pin) {
            Serial.println(F("Grove Air Quality 1.3 not configured"));
            return false;
        }
        _data_pin = data_pin;
        Serial.print(F("Setting Grove Air Quality 1.3 pin to: "));
        Serial.println(_data_pin, DEC);
        pinMode(_data_pin, INPUT);
        return true;
    }
    
    float read_legacy()
    {
        if (_data_pin) {
            return analogRead(_data_pin) * 3.3 / 1024;
        } else {
            return 0.0;
        }
    }

    int32_t read()
    {
        if (_data_pin) {
            return analogRead(_data_pin);
        } else {
            return 0;
        }
    }
}


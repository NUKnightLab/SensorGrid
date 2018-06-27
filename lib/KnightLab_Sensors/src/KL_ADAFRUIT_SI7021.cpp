#include "KL_ADAFRUIT_SI7021.h"

/* Adafruit Si7021 temperature/humidity breakout */


static TimeFunction _time_fcn;
static uint8_t _node_id;


namespace ADAFRUIT_SI7021 {

    static Adafruit_Si7021 sensor = Adafruit_Si7021();

    float readTemperature()
    {
        return sensor.readTemperature() * 1.8 + 32;
    }

    float readHumidity()
    {
        return sensor.readHumidity();
    }

    bool setup(uint8_t node_id, TimeFunction time_fcn)
    {
        _node_id = node_id;
        _time_fcn = time_fcn;
        
        Serial.print(F("Si7021 "));
        if (sensor.begin()) {
            Serial.println(F("Found"));
            return true;
        } else {
            Serial.println(F("Not Found"));
            return false;
        }
    }

    bool start()
    {
        return true;
    }


    size_t read(char* buf, int len)
    {
        char temp[7];
        char humid[7];
        ftoa(readTemperature(), temp, 2);
        ftoa(readHumidity(), humid, 2);
        //float temp = readTemperature();
        //float humid = readHumidity();
        snprintf(buf, len,
        "{\"node\":%d,\"tmp\":%s,\"hmd\":%s,\"ts\":%lu}",
        _node_id, temp, humid, _time_fcn());
        Serial.println(buf);
        return strlen(buf);
    }

    bool stop()
    {
        return true;
    }

    
}


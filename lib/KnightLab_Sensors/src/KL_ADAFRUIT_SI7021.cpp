#include "KL_ADAFRUIT_SI7021.h"

/* Adafruit Si7021 temperature/humidity breakout */


//static TimeFunction _time_fcn;
//static uint8_t _node_id;

static Adafruit_Si7021 sensor = Adafruit_Si7021();

float ADAFRUIT_SI7021::readTemperature() {
    return sensor.readTemperature() * 1.8 + 32;
}

float ADAFRUIT_SI7021::readHumidity() {
    return sensor.readHumidity();
}

bool ADAFRUIT_SI7021::setup(uint8_t node_id, TimeFunction time_fcn) {
    this._node_id = node_id;
    this._time_fcn = time_fcn;
    
    Serial.print(F("Si7021 "));
    if (sensor.begin()) {
        Serial.println(F("Found"));
        return true;
    } else {
        Serial.println(F("Not Found"));
        return false;
    }
}

bool ADAFRUIT_SI7021::start() {
    return true;
}

size_t ADAFRUIT_SI7021::read(char* buf, int len) {
    char temp[7];
    char humid[7];
    ftoa(readTemperature(), temp, 2);
    ftoa(readHumidity(), humid, 2);
    snprintf(buf, len,
    "{\"node\":%d,\"tmp\":%s,\"hmd\":%s,\"ts\":%lu}",
    this._node_id, temp, humid, this._time_fcn());
    Serial.println(buf);
    return strlen(buf);
}

bool ADAFRUIT_SI7021::stop() {
    return true;
}  
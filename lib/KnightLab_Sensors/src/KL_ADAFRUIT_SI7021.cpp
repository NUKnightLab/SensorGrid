#include "KL_ADAFRUIT_SI7021.h"

/* Adafruit Si7021 temperature/humidity breakout */

static Adafruit_Si7021 sensor = Adafruit_Si7021();

float ADAFRUIT_SI7021::readTemperature() {
    return sensor.readTemperature() * 1.8 + 32;
}

float ADAFRUIT_SI7021::readHumidity() {
    return sensor.readHumidity();
}

ADAFRUIT_SI7021::ADAFRUIT_SI7021(uint8_t node_id, TimeFunction time_fcn) {
    id = "SI7021_TEMP_HUMIDITY";
    _node_id = node_id;
    _time_fcn = time_fcn;
}

ADAFRUIT_SI7021::~ADAFRUIT_SI7021() {}

bool ADAFRUIT_SI7021::setup() {
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
    this->_node_id, temp, humid, this->_time_fcn());
    Serial.println(buf);
    return strlen(buf);
}

bool ADAFRUIT_SI7021::stop() {
    return true;
}

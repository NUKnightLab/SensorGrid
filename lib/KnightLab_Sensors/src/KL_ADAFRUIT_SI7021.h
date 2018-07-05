#ifndef __KNIGHTLAB__ADAFRUIT_SI7021_H
#define __KNIGHTLAB__ADAFRUIT_SI7021_H

#include <Arduino.h>
#include <KnightLab_ArduinoUtils.h>
#include <Adafruit_Si7021.h>
#include "base.h"

//typedef uint32_t (*TimeFunction)();



class ADAFRUIT_SI7021 : public SensorInterface {
public:
    id = "SI7021_TEMP_HUMIDITY";
    // static bool setup(uint8_t node_id, TimeFunction time_fcn);
    // static bool start();
    // static size_t read(char* buf, int len);
    // static bool stop();
	float readTemperature();
	float readHumidity();
};

#endif

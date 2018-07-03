#ifndef __KNIGHTLAB__ADAFRUIT_SI7021_H
#define __KNIGHTLAB__ADAFRUIT_SI7021_H

#include <Arduino.h>
#include <KnightLab_ArduinoUtils.h>
#include <Adafruit_Si7021.h>
#include "base.h"

//typedef uint32_t (*TimeFunction)();



class ADAFRUIT_SI7021 : public SensorInterface {
    TimeFunction _time_fcn;
    uint8_t _node_id;

public:
    ADAFRUIT_SI7021(uint8_t node_id, TimeFunction time_fcn);
    virtual ~ADAFRUIT_SI7021();
    bool setup();
    bool start();
    size_t read(char* buf, int len);
    bool stop();
	float readTemperature();
	float readHumidity();
};

#endif

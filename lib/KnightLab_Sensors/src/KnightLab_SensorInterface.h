#ifndef __KNIGHTLAB__SENSORINTERFACE_H
#define __KNIGHTLAB__SENSORINTERFACE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <KnightLab_ArduinoUtils.h>

#define MAX_SENSOR_ID_STR 30

typedef uint32_t (*TimeFunction)();

class SensorInterface {
public:
    virtual ~SensorInterface(){};
    char *id = 0;
    virtual bool setup() = 0;
	virtual bool start() = 0;
    virtual size_t read(char* buf, int len) = 0;
	virtual bool stop() = 0;
};

#endif
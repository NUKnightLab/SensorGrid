#ifndef __KNIGHTLAB__SENSORCONFIG_H
#define __KNIGHTLAB__SENSORCONFIG_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <KnightLab_ArduinoUtils.h>

#include "KnightLab_SensorInterface.h"
#include "KL_ADAFRUIT_SI7021.h"
#include "HONEYWELL_HPM.h"

#define MAX_SENSOR_ID_STR 30

extern void loadSensorConfig(uint8_t node_id, TimeFunction time_fcn);

struct SensorConfig {
    SensorInterface *sensor;
    struct SensorConfig *next;
};

#endif
/**
 * Copyright 2018 Northwestern University
 */
#include "KnightLab_SensorConfig.h"

struct SensorConfig *sensor_config_head;

static SensorConfig *getNextSensorConfig(SensorConfig *current_config) {
    SensorConfig *new_config;
    if (current_config == NULL) {
        new_config = new SensorConfig();
        sensor_config_head = new_config;
    } else {
        current_config->next = new SensorConfig();
        new_config = current_config->next;
    }
    return new_config;
}

void loadSensorConfig(uint8_t node_id, TimeFunction time_fcn) {
    // struct SensorConfig *current_config = new SensorConfig();
    SensorConfig *current_config = NULL;
    SensorInterface *sensor;

    /* Adafruit Si7021 temperature/humidity breakout */
    sensor = new ADAFRUIT_SI7021(node_id, time_fcn);
    if (sensor->setup()) {
        Serial.println("Configuring ADAFRUIT_SI7021");
        current_config = getNextSensorConfig(current_config);
        current_config->sensor = sensor;
    } else {
        delete sensor;
    }

    sensor = new HONEYWELL_HPM(node_id, time_fcn);
    if (sensor->setup()) {
        Serial.println("Configuring HONEYWELL_HPM");
        current_config = getNextSensorConfig(current_config);
        current_config->sensor = sensor;
    } else {
        delete sensor;
    }
}

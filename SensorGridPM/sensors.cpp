/**
 * Copyright 2018 Northwestern University
 */
#include "sensors.h"

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

void loadSensorConfig() {
    // struct SensorConfig *current_config = new SensorConfig();
    SensorConfig *current_config = NULL;
    SensorInterface *sensor;

    /* Adafruit Si7021 temperature/humidity breakout */
    sensor = new ADAFRUIT_SI7021(config.node_id, getTime);
    if (sensor->setup()) {
        current_config = getNextSensorConfig(current_config);
        current_config->sensor = sensor;
    } else {
        delete sensor;
    }

    sensor = new HONEYWELL_HPM(config.node_id, getTime);
    if (sensor->setup()) {
        current_config = getNextSensorConfig(current_config);
        current_config->sensor = sensor;
    } else {
        delete sensor;
    }
}

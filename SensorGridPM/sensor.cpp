/**
 * Copyright 2018 Northwestern University
 */
#include "sensor.h"

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

    /* Adafruit Si7021 temperature/humidity breakout */
    if (ADAFRUIT_SI7021::setup(config.node_id, getTime)) {
        current_config = getNextSensorConfig(current_config);
        snprintf(current_config->id, MAX_SENSOR_ID_STR, ID_SI7021_TEMP_HUMIDITY);
        current_config->start_function = &(ADAFRUIT_SI7021::start);
        current_config->read_function = &(ADAFRUIT_SI7021::read);
        current_config->stop_function = &(ADAFRUIT_SI7021::stop);
    }

    if (HONEYWELL_HPM::setup(config.node_id, getTime)) {
        current_config = getNextSensorConfig(current_config);
        snprintf(current_config->id, MAX_SENSOR_ID_STR, ID_HONEYWELL_HPM);
        current_config->start_function = &(HONEYWELL_HPM::start);
        current_config->read_function = &(HONEYWELL_HPM::read);
        current_config->stop_function = &(HONEYWELL_HPM::stop);
    }
}

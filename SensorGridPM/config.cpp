/**
 * Copyright 2018 Northwestern University
 */
#include "config.h"

struct SensorConfig *sensor_config_head;
struct Config config;


uint32_t getTime() {
    return rtcz.getEpoch();
}

/**
 * We pull both pins 8 and 19 HIGH during SD card read. The default configuration
 * on the integrated LoRa M0 uses pin 8 for RFM95 chip select, but the WiFi
 * collector module with a LoRa wing uses pin 19 as the chip select. Since we have
 * not yet read the config file, it could be either one.
 *
 * No other RFM95 CS alternates are supported at this time
 */


void loadConfig() {
    int default_rfm_cs = atoi(DEFAULT_RFM95_CS);
    int alt_rfm_cs = atoi(ALTERNATE_RFM95_CS);
    digitalWrite(default_rfm_cs, HIGH);
    digitalWrite(alt_rfm_cs, HIGH);
    if (!readSDConfig(CONFIG_FILE)) {
        digitalWrite(default_rfm_cs, LOW);
        digitalWrite(alt_rfm_cs, LOW);
        config.network_id = (uint32_t)(atoi(getConfig("NETWORK_ID")));
        config.node_id = (uint8_t)(atoi(getConfig("NODE_ID")));
        config.collector_id = (uint8_t)(atoi(getConfig("COLLECTOR_ID")));
        config.rf95_freq = static_cast<float>(atof(getConfig("RF95_FREQ")));
        config.tx_power = (uint8_t)(atoi(getConfig("TX_POWER")));
        config.sensorgrid_version = (uint8_t)atoi(
            getConfig("SENSORGRID_VERSION", DEFAULT_SENSORGRID_VERSION));
        config.log_file = getConfig("LOGFILE", DEFAULT_LOG_FILE);
        config.display_timeout = (uint32_t)(atoi(
            getConfig("DISPLAY_TIMEOUT", DEFAULT_DISPLAY_TIMEOUT)));
        config.node_type = (uint8_t)(atoi(getConfig("NODE_TYPE")));
        config.heartbeat_period = (uint32_t)(atoi(
            getConfig("HEARTBEAT_PERIOD", DEFAULT_HEARTBEAT_PERIOD)));
        config.sample_period = (uint32_t)(atoi(
            getConfig("SAMPLE_PERIOD", DEFAULT_SAMPLE_PERIOD)));
        config.collection_period = (uint32_t)(atoi(
            getConfig("COLLECTION_PERIOD", DEFAULT_COLLECTION_PERIOD)));

        /* WiFi collector configs */
        config.wifi_ssid = getConfig("WIFI_SSID");
        config.wifi_password = getConfig("WIFI_PASSWORD");
        config.api_host = getConfig("API_HOST");
        config.api_port = (uint16_t)(atoi(
            getConfig("API_PORT", DEFAULT_API_PORT)));

        /* radio and SD card pinouts */
        config.SD_CHIP_SELECT_PIN = (uint32_t)(atoi(
            getConfig("SD_CHIP_SELECT_PIN", DEFAULT_SD_CHIP_SELECT_PIN)));
        config.RFM95_CS = (uint32_t)(atoi(
            getConfig("RFM95_CS", DEFAULT_RFM95_CS)));
        config.RFM95_RST = (uint32_t)(atoi(
            getConfig("RFM95_RST", DEFAULT_RFM95_RST)));
        config.RFM95_INT = (uint32_t)(atoi(
            getConfig("RFM95_INT", DEFAULT_RFM95_INT)));

        /* Node IDs on collector */
        // char *node_ids_str[MAX_NODES] = {0};
        // config.node_ids[MAX_NODES] = {0};
    } else {
        logln(F("ERROR: No config file found"));
        fail(FAIL_CODE_CONFIG_READ);
    }
    logln(F("Config loaded"));
    if (!config.node_id) {
        logln(F("ERROR: Missing required config parameter NODE_ID"));
        fail(FAIL_CODE_BAD_CONFIG);
    }
}

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
        current_config->id = TYPE_SI7021_TEMP_HUMIDITY;
        snprintf(current_config->id_str, MAX_SENSOR_ID_STR, "SI7021_TEMP_HUMIDITY");
        current_config->start_function = &(ADAFRUIT_SI7021::start);
        current_config->read_function = &(ADAFRUIT_SI7021::read);
        current_config->stop_function = &(ADAFRUIT_SI7021::stop);
    }

    if (HONEYWELL_HPM::setup(config.node_id, getTime)) {
        current_config = getNextSensorConfig(current_config);
        current_config->id = TYPE_HONEYWELL_HPM;
        snprintf(current_config->id_str, MAX_SENSOR_ID_STR, "HONEYWELL_PM");
        current_config->start_function = &(HONEYWELL_HPM::start);
        current_config->read_function = &(HONEYWELL_HPM::read);
        current_config->stop_function = &(HONEYWELL_HPM::stop);
    }
}


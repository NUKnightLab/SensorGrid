#include "SensorGrid.h"
#include "config.h"

struct Config config;

void load_config() {
    if (!readSDConfig(CONFIG_FILE)) {
        config.network_id = (uint32_t)(atoi(getConfig("NETWORK_ID")));
        config.node_id = (uint32_t)(atoi(getConfig("NODE_ID")));
        config.rf95_freq = (float)(atof(getConfig("RF95_FREQ")));
        config.tx_power = (uint8_t)(atoi(getConfig("TX_POWER")));
        config.protocol_version = atof(getConfig("PROTOCOL_VERSION"));
        config.log_file = getConfig("LOGFILE", DEFAULT_LOG_FILE);
        config.log_mode = getConfig("LOGMODE", DEFAULT_LOG_MODE);
        config.display_timeout = (uint32_t)(atoi(getConfig("DISPLAY_TIMEOUT", DEFAULT_DISPLAY_TIMEOUT)));
        config.gps_module = getConfig("GPS_MODULE");
        config.has_oled = (uint8_t)(atoi(getConfig("DISPLAY", DEFAULT_OLED)));
        config.do_transmit = (uint8_t)(atoi(getConfig("TRANSMIT", DEFAULT_TRANSMIT)));
        config.node_type = (uint8_t)(atoi(getConfig("NODE_TYPE")));
        Serial.print("Set node type: "); Serial.println(config.node_type);
        config.collector_id = (uint32_t)(atoi(getConfig("COLLECTOR_ID", DEFAULT_COLLECTOR_ID)));
        SHARP_GP2Y1010AU0F::setDustPin((uint8_t)(atoi(getConfig("SHARP_GP2Y1010AU0F_DUST_PIN"))));
        GROVE_AIR_QUALITY_1_3_PIN = (uint8_t)(atoi(getConfig("GROVE_AIR_QUALITY_1_3_PIN")));
        config.charge_only = atoi(getConfig("CHARGE", "0"));

        char *node_ids_str[254] = {0};
        config.node_ids[254] = {0};
        
        if (config.node_type == NODE_TYPE_ORDERED_COLLECTOR) {
            char* nodeIdsConfig = strdup(getConfig("ORDERED_NODE_IDS", ""));
            if (nodeIdsConfig[0] == NULL) {
                Serial.println("BAD CONFIGURATION. Node type ORDERED_COLLECTOR requires ORDERED_NODE_IDS");
                while(1);
            }
            Serial.print("Node IDs: ");
            int index = 0;
            char *token;
            while( (token = strsep(&nodeIdsConfig,",")) != NULL ) {
                int node = atoi(token);
                Serial.print(node, DEC); Serial.print(" ");
                config.node_ids[index++] = node;
            }
            Serial.println("");
        }
    } else {
        Serial.println(F("Using default configs"));
        config.network_id = (uint32_t)(atoi(DEFAULT_NETWORK_ID));
        config.node_id = (uint32_t)(atoi(DEFAULT_NODE_ID));
        config.rf95_freq = (float)(atof(DEFAULT_RF95_FREQ));
        config.tx_power = (uint8_t)(atoi(DEFAULT_TX_POWER));
        config.protocol_version = (float)(atof(DEFAULT_PROTOCOL_VERSION));
        config.log_file = DEFAULT_LOG_FILE;
        config.log_mode = DEFAULT_LOG_MODE;
        config.display_timeout = (uint32_t)(atoi(DEFAULT_DISPLAY_TIMEOUT));
        config.has_oled = (uint8_t)(atoi(DEFAULT_OLED));
        config.do_transmit = (uint8_t)(atoi(DEFAULT_TRANSMIT));
        config.node_type = (uint8_t)(atoi(getConfig("NODE_TYPE")));
        config.collector_id = (uint32_t)(atoi(DEFAULT_COLLECTOR_ID));        
    }
}

void setup_sensors() {
    Serial.println("--- Initializing Sensors ---");

    /* Adafruit Si7021 temperature/humidity breakout */
    /*
    Serial.print(F("Si7021 "));
    if (sensorSi7021TempHumidity.begin()) {
        Serial.println(F("Found"));
        sensorSi7021Module = true;
    } else {
        Serial.println(F("Not Found"));
    }
    */

    /* Adafruit Si1145 IR/UV/Vis light breakout */
    /*
    Serial.print(F("Si1145 "));
    if (sensorSi1145UV.begin()) {
        Serial.println(F("Found"));
        sensorSi1145Module = true;
    } else {
        Serial.println(F("Not Found"));
    }
    */

    /* Sharp GP2Y1010AU0F dust */
    SHARP_GP2Y1010AU0F::setupDustSensor();

    /* Grove air quality 1.3 */
    if (GROVE_AIR_QUALITY_1_3_PIN) {
        setupGroveAirQualitySensor();
    }

}


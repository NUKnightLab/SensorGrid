#include "SensorGrid.h"
#include "config.h"

struct Config config;
struct SensorConfig *sensor_config_head;

/* We pull both pins 8 and 19 HIGH during SD card read. The default configuration
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
        config.node_id = (uint32_t)(atoi(getConfig("NODE_ID")));
        config.rf95_freq = (float)(atof(getConfig("RF95_FREQ")));
        config.tx_power = (uint8_t)(atoi(getConfig("TX_POWER")));
        config.sensorgrid_version = (uint8_t)atoi(getConfig("SENSORGRID_VERSION", DEFAULT_SENSORGRID_VERSION));
        config.log_file = getConfig("LOGFILE", DEFAULT_LOG_FILE);
        config.log_mode = getConfig("LOGMODE", DEFAULT_LOG_MODE);
        config.display_timeout = (uint32_t)(atoi(getConfig("DISPLAY_TIMEOUT", DEFAULT_DISPLAY_TIMEOUT)));
        config.gps_module = getConfig("GPS_MODULE");
        config.has_oled = (uint8_t)(atoi(getConfig("DISPLAY", DEFAULT_OLED)));
        config.do_transmit = (uint8_t)(atoi(getConfig("TRANSMIT", DEFAULT_TRANSMIT)));
        config.node_type = (uint8_t)(atoi(getConfig("NODE_TYPE")));
        config.collector_id = (uint32_t)(atoi(getConfig("COLLECTOR_ID", DEFAULT_COLLECTOR_ID)));
        config.charge_only = atoi(getConfig("CHARGE", "0"));
        config.collection_period = (uint32_t)(atoi(getConfig("COLLECTION_PERIOD", DEFAULT_COLLECTION_PERIOD)));

        /* WiFi collector configs */
        config.wifi_ssid = getConfig("WIFI_SSID");
        config.wifi_password = getConfig("WIFI_PASSWORD");
        config.api_host = getConfig("API_HOST");
        config.api_port = (uint16_t)(atoi(getConfig("API_PORT", DEFAULT_API_PORT)));

        /* sensor configs */
        config.SHARP_GP2Y1010AU0F_DUST_PIN = (uint8_t)(atoi(getConfig("SHARP_GP2Y1010AU0F_DUST_PIN")));
        config.GROVE_AIR_QUALITY_1_3_PIN = (uint8_t)(atoi(getConfig("GROVE_AIR_QUALITY_1_3_PIN")));

        /* radio and SD card pinouts */
        config.SD_CHIP_SELECT_PIN = (uint32_t)(atoi(getConfig("SD_CHIP_SELECT_PIN", DEFAULT_SD_CHIP_SELECT_PIN)));
        config.RFM95_CS = (uint32_t)(atoi(getConfig("RFM95_CS", DEFAULT_RFM95_CS)));
        config.RFM95_RST = (uint32_t)(atoi(getConfig("RFM95_RST", DEFAULT_RFM95_RST)));
        config.RFM95_INT = (uint32_t)(atoi(getConfig("RFM95_INT", DEFAULT_RFM95_INT)));

        /* Node IDs on collector */
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
        config.sensorgrid_version = (uint8_t)(atoi(DEFAULT_SENSORGRID_VERSION));
        config.log_file = DEFAULT_LOG_FILE;
        config.log_mode = DEFAULT_LOG_MODE;
        config.display_timeout = (uint32_t)(atoi(DEFAULT_DISPLAY_TIMEOUT));
        config.has_oled = (uint8_t)(atoi(DEFAULT_OLED));
        config.do_transmit = (uint8_t)(atoi(DEFAULT_TRANSMIT));
        config.node_type = (uint8_t)(atoi(getConfig("NODE_TYPE")));
        config.collector_id = (uint32_t)(atoi(DEFAULT_COLLECTOR_ID));
        config.collection_period = (uint32_t)(atoi(DEFAULT_COLLECTION_PERIOD));
        config.SD_CHIP_SELECT_PIN = (uint32_t)(atoi(DEFAULT_SD_CHIP_SELECT_PIN));
        config.RFM95_CS = (uint32_t)(atoi(DEFAULT_RFM95_CS));
        config.RFM95_RST = (uint32_t)(atoi(DEFAULT_RFM95_RST));
        config.RFM95_INT = (uint32_t)(atoi(DEFAULT_RFM95_INT));
    }
    Serial.println("Config loaded");
}

void setupSensors() {
    Serial.println("--- Initializing Sensors ---");

    struct SensorConfig *sensor_config = new SensorConfig();
    sensor_config_head = sensor_config;

    /* Adafruit Ultimate GPS FeatherWing */
    if (ADAFRUIT_ULTIMATE_GPS::setup()) {
        /* fix */
        sensor_config->next = new SensorConfig();
        sensor_config = sensor_config->next;
        sensor_config->id = "GPS_FIX";
        sensor_config->read_function = &(ADAFRUIT_ULTIMATE_GPS::fix);
        /* sats */
        sensor_config->next = new SensorConfig();
        sensor_config = sensor_config->next;
        sensor_config->id = "GPS_SATS";
        sensor_config->read_function = &(ADAFRUIT_ULTIMATE_GPS::satellites);
        /* satfix */
        sensor_config->next = new SensorConfig();
        sensor_config = sensor_config->next;
        sensor_config->id = "GPS_SATFIX";
        sensor_config->read_function = &(ADAFRUIT_ULTIMATE_GPS::satfix);
        /* latitude (degrees) */
        sensor_config->next = new SensorConfig();
        sensor_config = sensor_config->next;
        sensor_config->id = "GPS_LAT_DEG";
        sensor_config->read_function = &(ADAFRUIT_ULTIMATE_GPS::latitudeDegrees);
        /* longitude (degrees) */
        sensor_config->next = new SensorConfig();
        sensor_config = sensor_config->next;
        sensor_config->id = "GPS_LON_DEG";
        sensor_config->read_function = &(ADAFRUIT_ULTIMATE_GPS::longitudeDegrees);
    }

    /* Adafruit Si7021 temperature/humidity breakout */
    if (ADAFRUIT_SI7021::setup()) { 
        /* temperature */
        sensor_config->next = new SensorConfig();
        sensor_config = sensor_config->next;
        sensor_config->id = "SI7021_TEMP";
        sensor_config->read_function = &(ADAFRUIT_SI7021::readTemperature);
        /* humidity */
        sensor_config->next = new SensorConfig();
        sensor_config = sensor_config->next;
        sensor_config->id = "SI7021_HUMIDITY";
        sensor_config->read_function = &(ADAFRUIT_SI7021::readHumidity);
    }

    /* Adafruit Si1145 IR/UV/Visible light breakout */
    if (ADAFRUIT_SI1145::setup()) {
        /* visible light */
        sensor_config->next = new SensorConfig();
        sensor_config = sensor_config->next;
        sensor_config->id = "SI1145_VIS";
        sensor_config->read_function = &(ADAFRUIT_SI1145::readVisible);
        /* IR */
        sensor_config->next = new SensorConfig();
        sensor_config = sensor_config->next;
        sensor_config->id = "SI1145_IR";
        sensor_config->read_function = &(ADAFRUIT_SI1145::readIR);
        /* UV */
        sensor_config->next = new SensorConfig();
        sensor_config = sensor_config->next;
        sensor_config->id = "SI1145_UV";
        sensor_config->read_function = &(ADAFRUIT_SI1145::readUV);              
    }

    /* Sharp GP2Y1010AU0F dust */
    if (SHARP_GP2Y1010AU0F::setup(config.SHARP_GP2Y1010AU0F_DUST_PIN)) {
        sensor_config->next = new SensorConfig();
        sensor_config = sensor_config->next;
        sensor_config->id = "SHARP_GP2Y1010AU0F_DUST";
        sensor_config->read_function = &(SHARP_GP2Y1010AU0F::read);
    }

    /* Grove air quality 1.3 */
    if (GROVE_AIR_QUALITY_1_3::setup(config.GROVE_AIR_QUALITY_1_3_PIN)) {
        sensor_config->next = new SensorConfig();
        sensor_config = sensor_config->next;
        sensor_config->id = "GROVE_AIR_QUALITY_1_3";
        sensor_config->read_function = &(GROVE_AIR_QUALITY_1_3::read);
    }
}


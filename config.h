#ifndef _CONFIG_H
#define _CONFIG_H

#include "ADAFRUIT_ULTIMATE_GPS.h"
#include "ADAFRUIT_SI7021.h"
#include "ADAFRUIT_SI1145.h"
#include "SHARP_GP2Y1010AU0F.h"
#include "GROVE_AIR_QUALITY_1_3.h"


#define NODE_TYPE_COLLECTOR 1
#define NODE_TYPE_ROUTER 2
#define NODE_TYPE_SENSOR 3
#define NODE_TYPE_ORDERED_COLLECTOR 4
#define NODE_TYPE_ORDERED_SENSOR_ROUTER 5

/* Config defaults are strings so they can be passed to getConfig */
//char* DEFAULT_NETWORK_ID = "1";
#define DEFAULT_NETWORK_ID "1"
#define DEFAULT_NODE_ID "1"
#define DEFAULT_RF95_FREQ "915.0"  // for U.S.
#define DEFAULT_TX_POWER "10"
#define DEFAULT_PROTOCOL_VERSION "0.11"
#define DEFAULT_DISPLAY_TIMEOUT "60"
#define DEFAULT_COLLECTOR_ID "1"
#define DEFAULT_OLED "0"
#define DEFAULT_LOG_FILE "sensorgrid.log"
#define DEFAULT_TRANSMIT "1"
#define DEFAULT_LOG_MODE "NODE" // NONE, NODE, NETWORK, ALL
#define DEFAULT_COLLECTION_PERIOD "60" //defaults to 60 sec
#define DEFAULT_SD_CHIP_SELECT_PIN "10"
#define DEFAULT_RFM95_CS "8"
#define ALTERNATE_RFM95_CS "19"
#define DEFAULT_RFM95_RST "4"
#define DEFAULT_RFM95_INT "3"
#define DEFAULT_WIFI_SSID "Knight Lab"
#define DEFAULT_API_PORT "80"

typedef struct Config {
    uint32_t network_id;
    uint32_t node_id;
    uint32_t collector_id;
    float rf95_freq;
    uint8_t tx_power;
    uint16_t protocol_version;
    char  *log_file;
    char  *log_mode;
    uint32_t display_timeout;
    char  *gps_module;
    uint8_t has_oled;
    uint8_t do_transmit;
    uint8_t node_type;
    uint8_t charge_only;
    int node_ids[255];
    uint32_t collection_period; //in seconds
    uint32_t SD_CHIP_SELECT_PIN;
    uint32_t RFM95_CS;
    uint32_t RFM95_RST;
    uint32_t RFM95_INT;

    /* wifi collector */
    char *wifi_ssid;
    char *wifi_password;
    char *api_host;
    uint16_t api_port;

    /* sensors */
    uint8_t SHARP_GP2Y1010AU0F_DUST_PIN;
    uint8_t GROVE_AIR_QUALITY_1_3_PIN;
};

extern void loadConfig();
extern void setupSensors();
extern struct Config config;

typedef int32_t (*SensorReadFunction)();

typedef struct SensorConfig {
    char *id;
    SensorReadFunction read_function;
    struct SensorConfig *next;
};
extern struct SensorConfig *sensor_config_head;

#endif

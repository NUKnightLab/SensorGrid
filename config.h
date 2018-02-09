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
#define NODE_TYPE_SENSOR_LOGGER 6

/* data types */
#define TYPE_ADAFRUIT_ULTIMATE_GPS_FIX 1
#define TYPE_ADAFRUIT_ULTIMATE_GPS_SATS 2
#define TYPE_ADAFRUIT_ULTIMATE_GPS_SATFIX 3
#define TYPE_ADAFRUIT_ULTIMATE_GPS_LAT_DEG 4
#define TYPE_ADAFRUIT_ULTIMATE_GPS_LON_DEG 5
#define TYPE_ADAFRUIT_ULTIMATE_GPS_LOGGING 6
#define TYPE_SI7021_TEMP 7
#define TYPE_SI7021_HUMIDITY 8
#define TYPE_SI1145_VIS 9
#define TYPE_SI1145_IR 10
#define TYPE_SI1145_UV 11
#define TYPE_SHARP_GP2Y1010AU0F_DUST 12
#define TYPE_GROVE_AIR_QUALITY_1_3 13

/* Config defaults are strings so they can be passed to getConfig */
//char* DEFAULT_NETWORK_ID = "1";
#define DEFAULT_NETWORK_ID "1"
#define DEFAULT_NODE_ID "1"
#define DEFAULT_RF95_FREQ "915.0"  // for U.S.
#define DEFAULT_TX_POWER "10"
#define DEFAULT_SENSORGRID_VERSION "1"
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
#define DEFAULT_API_PORT "80"

typedef struct Config {
    uint32_t network_id;
    uint32_t node_id;
    uint32_t collector_id;
    float rf95_freq;
    uint8_t tx_power;
    uint8_t sensorgrid_version;
    char  *log_file;
    char  *log_mode;
    uint32_t display_timeout;
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

    /* GPS */
    uint16_t ADAFRUIT_ULTIMATE_GPS_FIX_PERIOD;
    uint16_t ADAFRUIT_ULTIMATE_GPS_SATS_PERIOD;
    uint16_t ADAFRUIT_ULTIMATE_GPS_SATFIX_PERIOD;
    uint16_t ADAFRUIT_ULTIMATE_GPS_LAT_DEG_PERIOD;
    uint16_t ADAFRUIT_ULTIMATE_GPS_LON_DEG_PERIOD;
    uint16_t ADAFRUIT_ULTIMATE_GPS_LOGGING_PERIOD;

    /* sensors */
    uint8_t SHARP_GP2Y1010AU0F_DUST_PIN;
    uint16_t SHARP_GP2Y1010AU0F_DUST_PERIOD;
    uint8_t GROVE_AIR_QUALITY_1_3_PIN;
    uint16_t GROVE_AIR_QUALITY_1_3_PERIOD;
};

extern void loadConfig();
extern void setupSensors();
extern struct Config config;

typedef int32_t (*SensorReadFunction)();

typedef struct SensorConfig {
    uint8_t id;
    char *id_str;
    SensorReadFunction read_function;
    uint16_t period;
    uint32_t last_sample_time;
    struct SensorConfig *next;
};
extern struct SensorConfig *sensor_config_head;

#endif

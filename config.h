#ifndef _CONFIG_H
#define _CONFIG_H

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
};

extern void load_config();
extern struct Config config;

#endif

/**
 * Copyright 2018 Northwestern University
 */
#ifndef KNIGHTLAB_SENSORGRID_SENSORGRIDPM_CONFIG_H_
#define KNIGHTLAB_SENSORGRID_SENSORGRIDPM_CONFIG_H_

//#include <RTCZero.h>
//#include <RTClib.h>
#include <KnightLab_ArduinoUtils.h>
#include <KnightLab_FeatherUtils.h>
#include <KnightLab_SDConfig.h>
#include <HONEYWELL_HPM.h>

/**
 * SensorGrid will not print to serial if USB is not attached. This can be
 * problematic when debugging timing sensitive issues, in which case set
 * ALWAYS_LOG to true
 */
#define ALWAYS_LOG false
#define DO_STANDBY 1

enum Mode { WAIT, INIT, SAMPLE, HEARTBEAT, COMMUNICATE, STANDBY };
extern enum Mode mode;

extern void _setup();
extern void _loop();

extern RTCZero rtcz;
//extern RTC_PCF8523 rtc;
extern uint32_t get_time();
// extern OLED oled;

#define FAIL_CODE_GENERAL 1
#define FAIL_CODE_BAD_CONFIG 2
#define FAIL_CODE_CONFIG_READ 3

#define NODE_TYPE_COLLECTOR 1
#define NODE_TYPE_SENSOR 2

//#define MAX_NODES 20

#define CONFIG_FILE "CONFIG.TXT"  // Adalogger doesn't seem to like underscores
                                  // in the name!!!

/* Config defaults are strings so they can be passed to getConfig */
// char* DEFAULT_NETWORK_ID = "1";
#define DEFAULT_NETWORK_ID "1"
#define DEFAULT_RF95_FREQ "915.0"  // for U.S.
#define DEFAULT_TX_POWER "13"
#define DEFAULT_SENSORGRID_VERSION "1"
#define DEFAULT_DISPLAY_TIMEOUT "60"
#define DEFAULT_LOG_FILE "sensorgrid.log"
#define DEFAULT_COLLECTION_PERIOD "60"  // defaults to 60 sec
#define DEFAULT_SD_CHIP_SELECT_PIN "10"
#define DEFAULT_RFM95_CS "8"
#define ALTERNATE_RFM95_CS "19"
#define DEFAULT_RFM95_RST "4"
#define DEFAULT_RFM95_INT "3"
#define DEFAULT_API_PORT "80"


struct Config {
    uint32_t network_id;
    uint32_t node_id;
    float rf95_freq;
    uint8_t tx_power;
    uint8_t sensorgrid_version;
    char* log_file;
    char* log_mode;
    uint32_t display_timeout;
    uint8_t node_type;
    int node_ids[255];
    uint32_t collection_period;  // in seconds
    uint32_t SD_CHIP_SELECT_PIN;
    uint32_t RFM95_CS;
    uint32_t RFM95_RST;
    uint32_t RFM95_INT;

    /* wifi collector */
    char* wifi_ssid;
    char* wifi_password;
    char* api_host;
    uint16_t api_port;
};

extern void loadConfig();
extern struct Config config;
extern int SAMPLE_PERIOD;

struct Message {
    uint8_t sensorgrid_version;
    uint8_t network_id;
    uint8_t from_node;
    uint8_t message_type;
    uint8_t len;
    char data[100];
} __attribute__((packed));

#endif  // KNIGHTLAB_SENSORGRID_SENSORGRIDPM_CONFIG_H_

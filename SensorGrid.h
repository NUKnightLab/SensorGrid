#ifndef SENSORGRID_H
#define SENSORGRID_H

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_FeatherOLED.h>
#include <RTClib.h>
#include <KnightLab_SDConfig.h>
#include <KnightLab_GPS.h>
#include <Arduino.h>
#include "io.h"
#include <WiFi101.h>
#include "protocol.h"

#define CAD_TIMEOUT 1000
#define TIMEOUT 2000
#define REQUIRED_RH_VERSION_MAJOR 1
#define REQUIRED_RH_VERSION_MINOR 82
#define MAX_MESSAGE_SIZE 255

/**
 * Overall max message size is somewhere between 244 and 247 bytes. 247 will cause invalid length error
 *
 * Note that these max sizes on the message structs are system specific due to struct padding. The values
 * here are specific to the Cortex M0
 *
 */
#define MAX_DATA_RECORDS 39
#define MAX_NODES 100



/* *
 *  Message types:
 *  Not using enum for message types to ensure small numeric size
 */
#define MESSAGE_TYPE_NO_MESSAGE 0
#define MESSAGE_TYPE_CONTROL 1 
#define MESSAGE_TYPE_DATA 2
#define MESSAGE_TYPE_FLEXIBLE_DATA 3

#define MESSAGE_TYPE_UNKNOWN -1
#define MESSAGE_TYPE_MESSAGE_ERROR -2
#define MESSAGE_TYPE_NONE_BUFFER_LOCK -3
#define MESSAGE_TYPE_WRONG_VERSION -4
#define MESSAGE_TYPE_WRONG_NETWORK -5 // for testing only. Normally we will just skip messages from other networks
/**
 * Control codes
 */
//#define CONTROL_SEND_DATA 1
#define CONTROL_NONE 1 // no-op used for testing
#define CONTROL_NEXT_ACTIVITY_TIME 2
#define CONTROL_ADD_NODE 3

/* Data types */
#define DATA_TYPE_NODE_COLLECTION_LIST 1
#define DATA_TYPE_NEXT_ACTIVITY_SECONDS 2
#define DATA_TYPE_BATTERY_LEVEL 3
#define DATA_TYPE_SHARP_GP2Y1010AU0F 4
#define DATA_TYPE_WARN_50_PCT_DATA_HISTORY 5

/* Module defs */
#include "WINC1500.h"

#define CONFIG_FILE "CONFIG.TXT" // Adalogger doesn't seem to like underscores in the name!!!
#define MAX_NETWORK_SIZE 100
#define LED 13
#define VBATPIN 9
#define BUTTON_A 9
#define BUTTON_B 6
#define BUTTON_C 5

#define CONTROL_TYPE_ACK 1
#define CONTROL_TYPE_SEND_DATA 2
#define CONTROL_TYPE_SLEEP 3

extern bool oled_is_on;
extern bool WiFiPresent;

extern RTC_PCF8523 rtc;
extern bool sensorSi1145Module;

enum ERRORS {
     NO_ERR,
     MESSAGE_STRUCT_TOO_LARGE,
     LORA_INIT_FAIL,
     LORA_FREQ_FAIL,
     WIFI_MODULE_NOT_DETECTED,
     FAILED_CONFIG_FILE_READ
};

#if defined(ARDUINO_ARCH_SAMD)

extern Adafruit_FeatherOLED display;
extern uint32_t display_clock_time;

typedef struct Control {
    uint8_t id;
    uint8_t code;
    uint8_t from_node;
    int16_t data;
    uint8_t nodes[MAX_NODES];
};

typedef struct Data {
    uint8_t id; // 1-255 indicates Data
    uint8_t node_id;
    uint32_t timestamp;
    int8_t type;
    int16_t value;
};

/*
typedef struct FlexibleData {
    uint8_t id; // 1-255 indicates Data
    uint8_t node_id;
    uint32_t timestamp;
    unsigned char data[];
}; */

typedef struct Message {
    uint8_t sensorgrid_version;
    uint8_t network_id;
    uint32_t timestamp;
    uint8_t data[];
};

/*
typedef struct Message {
    uint8_t sensorgrid_version;
    uint8_t network_id;
    uint8_t from_node;
    uint32_t timestamp;
    uint8_t message_type;
    uint8_t len;
    union {
      struct Control control;
      struct Data data[MAX_DATA_RECORDS];
      uint8_t flexdata[];
    };
};
*/

#else
    #error Unsupported architecture
#endif

/*
 * TODO on M0:
 * From: https://learn.adafruit.com/adafruit-feather-m0-radio-with-lora-radio-module?view=all
 * If you're used to AVR, you've probably used PROGMEM to let the compiler know you'd like to put a variable or string in flash memory to save on RAM. On the ARM, its a little easier, simply add const before the variable name:
const char str[] = "My very long string";
That string is now in FLASH. You can manipulate the string just like RAM data, the compiler will automatically read from FLASH so you dont need special progmem-knowledgeable functions.
You can verify where data is stored by printing out the address:
Serial.print("Address of str $"); Serial.println((int)&str, HEX);
If the address is $2000000 or larger, its in SRAM. If the address is between $0000 and $3FFFF Then it is in FLASH
*/

class __FlashStringHelper;
#define F(string_literal) (reinterpret_cast<const __FlashStringHelper*>(PSTR(string_literal)))

void fail(enum ERRORS err);
void flashLED(int times, int endState);
float batteryLevel();
void printRam();
int freeRam();
void aButton_ISR();
void p(char *fmt, ...);
void p(const __FlashStringHelper *fmt, ... );
void output(char *fmt, ...);
void output(const __FlashStringHelper *fmt, ... );

#endif

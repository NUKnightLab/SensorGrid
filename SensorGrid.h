#ifndef SENSORGRID_H
#define SENSORGRID_H

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_FeatherOLED.h>
#include <RTClib.h>
#include <KnightLab_SDConfig.h>
#include <KnightLab_GPS.h>
#include "io.h"

/* Module defs */
#include "WINC1500.h"

#define CONFIG_FILE "CONFIG.TXT" // Adalogger doesn't seem to like underscores in the name!!!
#define MAX_NETWORK_SIZE 100
#define LED 13
#define VBATPIN A7
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

typedef struct Message {
    uint16_t ver;
    uint16_t net;
    uint16_t bat_100;
    uint32_t timestamp;
    uint16_t ram;
    int32_t data[10]; /* -2147483648 through 2147483647 */
};

typedef struct Control {
    uint8_t type;
    uint32_t data;
};

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

#endif

#ifndef SENSORGRID_H
#define SENSORGRID_H

#include <KnightLab_SDConfig.h>
#define CONFIG_FILE "CONFIG.TXT" // Adalogger doesn't seem to like underscores in the name!!!

#define NONE 0


/* Module defs */
#include "modules/wifi/WINC1500.h"

// Dust sensors
#define SHARP_GP2Y1010AU0F 1

#include "config.h"

extern uint32_t NETWORK_ID;
extern uint32_t NODE_ID;
extern char* LOGFILE;

enum ERRORS {
     NO_ERR,
     MESSAGE_STRUCT_TOO_LARGE,
     LORA_INIT_FAIL,
     LORA_FREQ_FAIL,
     WIFI_MODULE_NOT_DETECTED,
     FAILED_CONFIG_FILE_READ
};

#include "Adafruit_SI1145.h"
#include "Adafruit_Si7021.h"

#include "io.h"
#include <KnightLab_GPS.h>

#if defined(__AVR_ATmega32U4__)
typedef struct Message {
    uint16_t ver_100;
    uint16_t net;
    uint16_t snd;
    uint16_t orig;
    uint32_t id;
    uint16_t bat_100;
    uint8_t year, month, day, hour, minute, seconds;
    bool fix;
    uint8_t _padding1, _padding2, _padding3;
    int32_t lat_1000, lon_1000;
    uint8_t sats;
    uint8_t _padding4, _padding5, _padding6;
    int32_t data[10]; /* -2147483648 through 2147483647 */
};
#elif defined(ARDUINO_ARCH_SAMD)
typedef struct Message {
    uint16_t ver_100;
    uint16_t net;
    uint16_t snd;
    uint16_t orig;
    uint32_t id;
    uint16_t bat_100;
    uint8_t year, month, day, hour, minute, seconds;
    bool fix;
    int32_t lat_1000, lon_1000;
    uint8_t sats;
    int32_t data[10]; /* -2147483648 through 2147483647 */
};
#else
    #error Unsupported architecture
#endif
extern bool WiFiPresent;


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

#define LED 13
#if defined(__AVR_ATmega32U4__)
    #define VBATPIN A9
#elif defined(ARDUINO_ARCH_SAMD)
    #define VBATPIN A7
#endif

/*
#if defined(ARDUINO_SAMD_ZERO) && defined(SERIAL_PORT_USBVIRTUAL)
    // Required for Serial on Zero based boards
    #define Serial SERIAL_PORT_USBVIRTUAL
#endif
*/

class __FlashStringHelper;
#define F(string_literal) (reinterpret_cast<const __FlashStringHelper*>(PSTR(string_literal)))

void fail(enum ERRORS err);
void flashLED(int times, int endState);
float batteryLevel();
void printRam();

extern Adafruit_Si7021 sensorSi7021TempHumidity;
extern Adafruit_SI1145 sensorSi1145UV;
extern bool sensorSi7021Module;
extern bool sensorSi1145Module;

#endif

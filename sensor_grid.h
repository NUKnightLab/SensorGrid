#ifndef SENSORGRID_H
#define SENSORGRID_H

#define NONE 0

// Boards / Chipsets
#define Feather32u4 1
#define FeatherM0 2

/* Module defs */

// WiFi modules
#define WINC1500 1

// Dust sensors
#define SHARP_GP2Y1010AU0F 1

enum ERRORS {
     NO_ERR,
     MESSAGE_STRUCT_TOO_LARGE,
     LORA_INIT_FAIL,
     LORA_FREQ_FAIL,
     WIFI_MODULE_NOT_DETECTED
};

#include "config.h"
#include "modules/gps/GPS.h"
#include "encryption.h"

void fail(enum ERRORS err) {
    Serial.print(F("ERR: "));
    Serial.println(err);
    while(1);
}

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

#if defined(ARDUINO_SAMD_ZERO) && defined(SERIAL_PORT_USBVIRTUAL)
    // Required for Serial on Zero based boards
    #define Serial SERIAL_PORT_USBVIRTUAL
#endif

class __FlashStringHelper;
#define F(string_literal) (reinterpret_cast<const __FlashStringHelper*>(PSTR(string_literal)))

void flashLED(int times, int endState) {
    digitalWrite(LED, LOW); delay(50);
    for (int i=0; i<times; i++) {
        digitalWrite(LED, HIGH); delay(50);
        digitalWrite(LED, LOW); delay(50);
    }
    digitalWrite(LED, endState);
}

float batteryLevel() {
    float measuredvbat = analogRead(VBATPIN);
    measuredvbat *= 2;    // we divided by 2, so multiply back
    measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
    measuredvbat /= 1024; // convert to voltage
    return measuredvbat;
}

#if defined(__AVR_ATmega32U4__)
int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}
#elif defined(ARDUINO_ARCH_SAMD)
extern "C" char *sbrk(int i);
int freeRam() {
  char stack_dummy = 0;
  return &stack_dummy - sbrk(0);
}
#endif

void printRam() {
    Serial.print(F("freeRam: "));
    Serial.println(freeRam());
}

#endif

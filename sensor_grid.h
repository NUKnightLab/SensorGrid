#ifndef SENSORGRID_H
#define SENSORGRID_H

#include "defs.h"
#include "errors.h"
#include "config.h"

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

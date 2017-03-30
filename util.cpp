#include "SensorGrid.h"

#if defined(__AVR_ATmega32U4__)
static int _freeRam () {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}
#elif defined(ARDUINO_ARCH_SAMD)
extern "C" char *sbrk(int i);
static int _freeRam() {
  char stack_dummy = 0;
  return &stack_dummy - sbrk(0);
}
#endif

void fail(enum ERRORS err) {
    Serial.print(F("ERR: "));
    Serial.println(err);
    while(1);
}

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

void printRam() {
    Serial.print(F("Free RAM: "));
    Serial.println(_freeRam());
}

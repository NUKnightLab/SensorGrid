#include "SensorGrid.h"

#if defined(__AVR_ATmega32U4__)
/* Here for reference. 32u4 no longer supported */
static int _freeRam () {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}
#elif defined(ARDUINO_ARCH_SAMD)
extern "C" char *sbrk(int i);
static int _freeRam()
{
  char stack_dummy = 0;
  return &stack_dummy - sbrk(0);
}
#endif


#include <stdarg.h>
void p(char *fmt, ... ){
        Serial.print(rtc.now().unixtime());
        Serial.print(": ");
        char buf[128]; // resulting string limited to 128 chars
        va_list args;
        va_start (args, fmt );
        vsnprintf(buf, 128, fmt, args);
        va_end (args);
        Serial.print(buf);
}

void p(const __FlashStringHelper *fmt, ... ){
  Serial.print(rtc.now().unixtime());
  Serial.print(": ");
  char buf[128]; // resulting string limited to 128 chars
  va_list args;
  va_start (args, fmt);
#ifdef __AVR__
  vsnprintf_P(buf, sizeof(buf), (const char *)fmt, args); // progmem for AVR
#else
  vsnprintf(buf, sizeof(buf), (const char *)fmt, args); // for the rest of the world
#endif
  va_end(args);
  Serial.print(buf);
}

void fail(enum ERRORS err)
{
    Serial.print(F("ERR: "));
    Serial.println(err);
    while(1);
}

void flashLED(int times, int endState)
{
    digitalWrite(LED, LOW); delay(50);
    for (int i=0; i<times; i++) {
        digitalWrite(LED, HIGH); delay(50);
        digitalWrite(LED, LOW); delay(50);
    }
    digitalWrite(LED, endState);
}

float batteryLevel()
{
    pinMode(BUTTON_A, INPUT);
    float measuredvbat = analogRead(VBATPIN);
    pinMode(BUTTON_A, INPUT_PULLUP);
    attachInterrupt(BUTTON_A, aButton_ISR, CHANGE);
    measuredvbat *= 2;    // we divided by 2, so multiply back
    measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
    measuredvbat /= 1024; // convert to voltage
    return measuredvbat;
}

void printRam()
{
    Serial.print(F("Free RAM: "));
    Serial.println(_freeRam());
}

int freeRam()
{
    return _freeRam();
}

unsigned long hash(uint8_t* msg, uint8_t len)
{
    unsigned long h = 5381;
    for (int i=0; i<len; i++){
        h = ((h << 5) + h) + msg[i];
    }
    return h;
}

unsigned checksum(void *buffer, size_t len, unsigned int seed)
{
      unsigned char *buf = (unsigned char *)buffer;
      size_t i;
      for (i = 0; i < len; ++i)
            seed += (unsigned int)(*buf++);
      return seed;
}



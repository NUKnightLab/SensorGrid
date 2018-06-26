#include "KnightLab_ArduinoUtils.h"

void fail(int code)
{
    while(1) {
        for (int i=0; i<code; i++) {
            digitalWrite(LED_BUILTIN, HIGH);
            delay(200);
            digitalWrite(LED_BUILTIN, LOW);
            delay(100);
        }
        delay(1000);
    }
}



/**
 *  printing / logging functions
 */

static bool logging_on = false;
static char buf[LOGGING_BUFFER_SIZE];

/**
 * For M0, internal RTC is used for timestamps. The RTC should be initialized
 * and set before making calls to this library.
 */
#ifdef ARDUINO_ARCH_SAMD
static RTCZero rtc;
#endif

void set_logging(bool b)
{
    logging_on = b;
}

void ts(){
#ifdef ARDUINO_ARCH_SAMD
    print(F("20%02d-%02d-%02dT%02d:%02d:%02d: "),
        rtc.getYear(), rtc.getMonth(), rtc.getDay(),
        rtc.getHours(), rtc.getMinutes(), rtc.getSeconds());
#else
    print(F("%d: "), millis());
#endif
}


#ifdef __AVR__
#define PRINT(fmt, ...) ({\
    va_list args;\
    va_start (args, fmt);\
    vsnprintf_P(buf, LOGGING_BUFFER_SIZE, (const char *)fmt, args);\
    va_end(args);\
    Serial.print(buf);\
})
#else
#define PRINT(fmt, ...) ({\
    va_list args;\
    va_start (args, fmt);\
    vsnprintf(buf, LOGGING_BUFFER_SIZE, (const char *)fmt, args);\
    va_end(args);\
    Serial.print(buf);\
})
#endif

void print(char *fmt, ... )
{
    if (!logging_on) return;
    PRINT(fmt, ...);
}

void print(const __FlashStringHelper *fmt, ...)
{
    if (!logging_on) return;
    PRINT(fmt, ...);
}

void println(char *fmt, ...)
{
    if (!logging_on) return;
    PRINT(fmt, ...);
    Serial.println();
}

void println(const __FlashStringHelper *fmt, ... )
{
    if (!logging_on) return;
    PRINT(fmt, ...);
    Serial.println();
}

void log_(char *fmt, ... )
{
    if (!logging_on) return;
    PRINT(F("%d: "), millis());
    PRINT(fmt, ...);
}

void log_(const __FlashStringHelper *fmt, ... )
{
    if (!logging_on) return;
    ts();
    PRINT(fmt, ...);
}

void logln(char *fmt, ... ){
    if (!logging_on) return;
    ts();
    PRINT(fmt, ...);
    Serial.println();
}

void logln(const __FlashStringHelper *fmt, ... )
{
    if (!logging_on) return;
    ts();
    PRINT(fmt, ...);
    Serial.println();
}

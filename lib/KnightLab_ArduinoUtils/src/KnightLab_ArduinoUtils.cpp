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

// ftoa functions from: https://www.geeksforgeeks.org/convert-floating-point-number-string/
// reverses a string 'str' of length 'len'
void reverse(char *str, int len) {
    int i=0, j=len-1, temp;
    while (i<j) {
        temp = str[i];
        str[i] = str[j];
        str[j] = temp;
        i++; j--;
    }
}
 
 // Converts a given integer x to string str[].  d is the number
 // of digits required in output. If d is more than the number
 // of digits in x, then 0s are added at the beginning.
int intToStr(int x, char str[], int d) {
    int i = 0;
    while (x) {
        str[i++] = (x%10) + '0';
        x = x/10;
    }
    // If number of digits required is more, then
    // add 0s at the beginning
    while (i < d) {
        str[i++] = '0';
    }
    reverse(str, i);
    str[i] = '\0';
    return i;
}

// Converts a floating point number to string.
void ftoa(float n, char *res, int afterpoint) {
    // Extract integer part
    int ipart = (int)n;
    // Extract floating part
    float fpart = n - (float)ipart;
    // convert integer part to string
    int i = intToStr(ipart, res, 0);
    // check for display option after point
    if (afterpoint != 0) {
        res[i] = '.';  // add dot
        // Get the value of fraction part upto given no.
        // of points after dot. The third parameter is needed
        // to handle cases like 233.007
        fpart = fpart * pow(10, afterpoint);
        intToStr((int)fpart, res + i + 1, afterpoint);
    }
}
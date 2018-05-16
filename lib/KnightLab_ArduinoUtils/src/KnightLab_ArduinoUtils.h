#ifndef __KL_ARDUINO_UTILS__
#define __KL_ARDUINO_UTILS__

#include <Arduino.h>
#include <stdarg.h>

#ifdef ARDUINO_ARCH_SAMD
#include <RTCZero.h>
#endif

#define WAIT_SERIAL_TIMEOUT 10000
#define LOGGING_BUFFER_SIZE 128

extern void set_logging(bool b);
extern bool is_logging();
extern void fail(int code);

class __FlashStringHelper;
//#define F(string_literal) (reinterpret_cast<const __FlashStringHelper*>(PSTR(string_literal)))

 /**
  * Some really basic utilities for printing / logging (with timestamps) to serial
  *
  * For M0, internal RTC is used for timestamps. The RTC should be initialized
  * and set before calling any of the logging functions
  *
  * Note: log_ has a dumb underscore to avoid conflicting with the mathematical
  * log function. I am open to better ideas for the name of this function. -SBB
  */
extern void print(char *fmt, ... );
extern void print(const __FlashStringHelper *fmt, ... );
extern void println(char *fmt, ... );
extern void println(const __FlashStringHelper *fmt, ... );
extern void log_(char *fmt, ... );
extern void log_(const __FlashStringHelper *fmt, ... );
extern void logln(char *fmt, ... );
extern void logln(const __FlashStringHelper *fmt, ... );

#endif

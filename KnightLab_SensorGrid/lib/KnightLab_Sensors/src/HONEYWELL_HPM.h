#ifndef __SENSORGRID_HONEYWELL_HPM__
#define __SENSORGRID_HONEYWELL_HPM__

#include <Arduino.h>
#include <ArduinoJson.h>

#define UART_TIMEOUT 1000

typedef uint32_t (*TimeFunction)();

namespace HONEYWELL_HPM {
    bool setup(uint8_t data_pin, TimeFunction time_fcn);
    bool start();
    int32_t read(char* buf, int len);
    bool stop();
}

#endif

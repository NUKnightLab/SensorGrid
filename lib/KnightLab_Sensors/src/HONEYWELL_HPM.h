#ifndef __SENSORGRID_HONEYWELL_HPM__
#define __SENSORGRID_HONEYWELL_HPM__

#include <Arduino.h>
#include <ArduinoJson.h>
#include <KnightLab_ArduinoUtils.h>
#include "base.h"

// don't set this to < 2000 w/o testing valid results from sensor
#define UART_TIMEOUT 500

//typedef uint32_t (*TimeFunction)();

class HONEYWELL_HPM : public Interface {
public:
    static bool setup(uint8_t node_id, TimeFunction time_fcn);
    static bool start();
    static size_t read(char* buf, int len);
    static bool stop();
};

#endif

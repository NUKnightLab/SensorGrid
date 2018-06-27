#ifndef __SENSORGRID_HONEYWELL_HPM__
#define __SENSORGRID_HONEYWELL_HPM__

#include <Arduino.h>
#include <ArduinoJson.h>

// don't set this to < 2000 w/o testing valid results from sensor
#define UART_TIMEOUT 500

typedef uint32_t (*TimeFunction)();

namespace HONEYWELL_HPM {
    bool setup(uint8_t node_id, TimeFunction time_fcn);
    bool start();
    size_t read(char* buf, int len);
//    void readData(JsonArray &data_array);
//    size_t readDataSample(char* buf, int len);
    bool stop();
}

#endif

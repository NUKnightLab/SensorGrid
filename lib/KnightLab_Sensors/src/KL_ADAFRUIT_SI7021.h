#ifndef __KNIGHTLAB__ADAFRUIT_SI7021_H
#define __KNIGHTLAB__ADAFRUIT_SI7021_H

#include <Arduino.h>
#include <KnightLab_ArduinoUtils.h>
#include <Adafruit_Si7021.h>

typedef uint32_t (*TimeFunction)();

namespace ADAFRUIT_SI7021 {
    bool setup(uint8_t node_id, TimeFunction time_fcn);
    //float readTemperature();
    //float readHumidity();
    size_t read(char* buf, int len);
    bool start();
    bool stop();
}

#endif

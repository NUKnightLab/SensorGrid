#ifndef __SENSORGRID_HONEYWELL_HPM__
#define __SENSORGRID_HONEYWELL_HPM__

#include "KnightLab_SensorInterface.h"

// don't set this to < 2000 w/o testing valid results from sensor
#define UART_TIMEOUT 2000

class HONEYWELL_HPM : public SensorInterface {
    TimeFunction _time_fcn;
    uint8_t _node_id;

public:
    HONEYWELL_HPM(uint8_t node_id, TimeFunction time_fcn);
    virtual ~HONEYWELL_HPM();
    bool setup();
    bool start();
    size_t read(char* buf, int len);
    //size_t read(char buf[], int len);
    bool stop();

};

#endif

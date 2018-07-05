#ifndef __KNIGHTLAB__BASE_H
#define __KNIGHTLAB__BASE_H

#define MAX_SENSOR_ID_STR 30

typedef uint32_t (*TimeFunction)();

class SensorInterface {
public:
    virtual char id[MAX_SENSOR_ID_STR];
    virtual bool setup(uint8_t node_id, TimeFunction time_fcn);
	virtual bool start();
    virtual size_t read(char* buf, int len);
	virtual bool stop();

private:
    virtual TimeFunction _time_fcn;
    virtual uint8_t _node_id;
};

#endif
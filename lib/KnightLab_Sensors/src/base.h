#ifndef __KNIGHTLAB__BASE_H
#define __KNIGHTLAB__BASE_H

#define MAX_SENSOR_ID_STR 30

typedef uint32_t (*TimeFunction)();

class SensorInterface {
public:
    virtual ~SensorInterface(){};
    char *id = 0;
    virtual bool setup() = 0;
	virtual bool start() = 0;
    virtual size_t read(char* buf, int len) = 0;
	virtual bool stop() = 0;
};

#endif
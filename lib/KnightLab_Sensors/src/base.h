#ifndef __KNIGHTLAB__BASE_H
#define __KNIGHTLAB__BASE_H


typedef uint32_t (*TimeFunction)();

class Interface {
public:
	virtual bool setup();
	//virtual bool start();
	virtual size_t read();
	//virtual bool stop();
};

#endif
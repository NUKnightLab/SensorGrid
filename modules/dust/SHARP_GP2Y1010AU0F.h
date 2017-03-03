#ifndef SHARP_GP2Y1010AU0F_H
#define SHARP_GP2Y1010AU0F_H

/* Adapted from:
	http://arduinodev.woofex.net/2012/12/01/standalone-sharp-dust-sensor/
	& http://www.howmuchsnow.com/arduino/airquality/
*/

//#include <Arduino.h>
#include "../../sensor_grid.h"

#define DUST_SENSOR_LED_POWER 12
#define DUST_SENSOR_READ A0
#define DUST_SENSOR_SAMPLING_TIME 280
#define DUST_SENSOR_DELTA_TIME 40
#define DUST_SENSOR_SLEEP_TIME 9680
#define DUST_SENSOR_VCC 3.3
#define DUST_SENSOR_LED_ON LOW
#define DUST_SENSOR_LED_OFF HIGH

extern float dustSenseVoMeasured;
extern float dustSenseCalcVoltage;
extern float dustDensity;

void setupDustSensor();
float readDustSensor();

#endif

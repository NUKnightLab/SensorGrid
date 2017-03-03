#include "SHARP_GP2Y1010AU0F.h"

float dustSenseVoMeasured = 0;
float dustSenseCalcVoltage = 0;
float dustDensity = 0;

void setupDustSensor() {
    #ifdef DEBUG
		Serial.println(F("Setting up Dust Sensor"));
    #endif
    pinMode(DUST_SENSOR_LED_POWER,OUTPUT);
    pinMode(DUST_SENSOR_READ, INPUT);
}

float readDustSensor() {
    digitalWrite(DUST_SENSOR_LED_POWER, DUST_SENSOR_LED_ON);
    delayMicroseconds(DUST_SENSOR_SAMPLING_TIME);
    dustSenseVoMeasured = analogRead(DUST_SENSOR_READ);
    delayMicroseconds(DUST_SENSOR_DELTA_TIME);
    digitalWrite(DUST_SENSOR_LED_POWER, DUST_SENSOR_LED_OFF);
    delayMicroseconds(DUST_SENSOR_SLEEP_TIME);
    dustSenseCalcVoltage = dustSenseVoMeasured * (DUST_SENSOR_VCC / 1024);
    dustDensity = 0.17 * dustSenseCalcVoltage - 0.1;
    #ifdef DEBUG
    	Serial.print(F("Raw Signal Value (0-1023): "));
    	Serial.print(dustSenseVoMeasured);
    	Serial.print(F(" - Voltage: "));
    	Serial.print(dustSenseCalcVoltage);
    	Serial.print(F(" - Dust Density: "));
    	Serial.println(dustDensity);
    #endif
	return dustDensity;
}

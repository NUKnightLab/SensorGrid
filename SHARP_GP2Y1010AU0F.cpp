/* Adapted from:
    http://arduinodev.woofex.net/2012/12/01/standalone-sharp-dust-sensor/
    & http://www.howmuchsnow.com/arduino/airquality/
*/
#include "SHARP_GP2Y1010AU0F.h"

float dustSenseVoMeasured = 0;
float dustSenseCalcVoltage = 0;
float dustDensity = 0;

void setupDustSensor()
{
    Serial.print(F("Setting Sharp GP2Y1010AU0F pin to: "));
    Serial.println(SHARP_GP2Y1010AU0F_DUST_PIN);
    pinMode(DUST_SENSOR_LED_POWER, OUTPUT);
    pinMode(SHARP_GP2Y1010AU0F_DUST_PIN, INPUT);
}

float readDustSensor()
{
    digitalWrite(DUST_SENSOR_LED_POWER, DUST_SENSOR_LED_ON);
    delayMicroseconds(DUST_SENSOR_SAMPLING_TIME);
    dustSenseVoMeasured = analogRead(SHARP_GP2Y1010AU0F_DUST_PIN);
    delayMicroseconds(DUST_SENSOR_DELTA_TIME);
    digitalWrite(DUST_SENSOR_LED_POWER, DUST_SENSOR_LED_OFF);
    delayMicroseconds(DUST_SENSOR_SLEEP_TIME);
    dustSenseCalcVoltage = dustSenseVoMeasured * (DUST_SENSOR_VCC / 1024);
    dustDensity = 0.17 * dustSenseCalcVoltage - 0.1;
    Serial.print(F("Raw Signal Value (0-1023): "));
    Serial.print(dustSenseVoMeasured, DEC);
    Serial.print(F(" - Voltage: "));
    Serial.print(dustSenseCalcVoltage);
    Serial.print(F(" - Dust Density: "));
    Serial.println(dustDensity);
    return dustDensity;
}

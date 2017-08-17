#include "GroveAirQuality.h"

/* TODO: do we need to consider historical averages and slope? See tutorial
 * at: http://wiki.seeed.cc/Grove-Air_Quality_Sensor_v1.3/ and AirQuality
 * library code: https://github.com/SeeedDocument/Grove_Air_Quality_Sensor_v1.3/raw/master/res/AirQuality_Sensor.zip
 */

void setupGroveAirQualitySensor()
{
    Serial.print("Setting Grove Air Quality 1.3 pin to: ");
    Serial.println(GROVE_AIR_QUALITY_1_3_PIN, DEC);
    pinMode(GROVE_AIR_QUALITY_1_3_PIN, INPUT);
}

float readGroveAirQualitySensor()
{
    return analogRead(GROVE_AIR_QUALITY_1_3_PIN) * 3.3 / 1024;
}


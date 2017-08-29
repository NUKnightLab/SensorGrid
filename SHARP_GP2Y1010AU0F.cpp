/* Adapted from:
    http://arduinodev.woofex.net/2012/12/01/standalone-sharp-dust-sensor/
    & http://www.howmuchsnow.com/arduino/airquality/
*/
#include "SHARP_GP2Y1010AU0F.h"

namespace SHARP_GP2Y1010AU0F {

    static uint8_t _data_pin = 0;
 
    bool setup(uint8_t data_pin)
    {
        if (!data_pin) {
            Serial.println(F("Sharp GP2Y1010AU0F  not configured"));
            return false;
        }
        _data_pin = data_pin;
        Serial.print(F("Setting Sharp GP2Y1010AU0F data pin to: "));
        Serial.println(_data_pin, DEC);
        pinMode(DUST_SENSOR_LED_POWER, OUTPUT);
        pinMode(_data_pin, INPUT);
        return true;
    }

    /**
     * Legacy read function returns a float value
     */
    float read_legacy()
    {
        if (_data_pin) {
            static float dust_sense_vo_measured = 0;
            static float dust_sense_calc_voltage = 0;
            static float dust_density = 0;
            digitalWrite(DUST_SENSOR_LED_POWER, DUST_SENSOR_LED_ON);
            delayMicroseconds(DUST_SENSOR_SAMPLING_TIME);
            dust_sense_vo_measured = analogRead(_data_pin);
            delayMicroseconds(DUST_SENSOR_DELTA_TIME);
            digitalWrite(DUST_SENSOR_LED_POWER, DUST_SENSOR_LED_OFF);
            delayMicroseconds(DUST_SENSOR_SLEEP_TIME);
            dust_sense_calc_voltage = dust_sense_vo_measured * (DUST_SENSOR_VCC / 1024);
            dust_density = 0.17 * dust_sense_calc_voltage - 0.1;
            Serial.print(F("Raw Signal Value (0-1023): "));
            Serial.print(dust_sense_vo_measured, DEC);
            Serial.print(F(" - Voltage: "));
            Serial.print(dust_sense_calc_voltage);
            Serial.print(F(" - Dust Density: "));
            Serial.println(dust_density);
            return dust_density;
        } else {
            return 0.0;
        }
    }

    /**
     * Get the analogRead value from the dust data pin
     */
    int32_t read() {
        if (_data_pin) {
            static float dust_sense_vo_measured = 0;
            digitalWrite(DUST_SENSOR_LED_POWER, DUST_SENSOR_LED_ON);
            delayMicroseconds(DUST_SENSOR_SAMPLING_TIME);
            dust_sense_vo_measured = analogRead(_data_pin);
            delayMicroseconds(DUST_SENSOR_DELTA_TIME);
            digitalWrite(DUST_SENSOR_LED_POWER, DUST_SENSOR_LED_OFF);
            delayMicroseconds(DUST_SENSOR_SLEEP_TIME);
            return dust_sense_vo_measured;
        } else {
            return 0;
        }
    }
}

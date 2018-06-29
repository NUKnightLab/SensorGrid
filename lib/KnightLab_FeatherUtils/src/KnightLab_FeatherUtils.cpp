#include "KnightLab_FeatherUtils.h"


float batteryLevel()
{
    float val = analogRead(VBATPIN);
    val *= 2;    // we divided by 2, so multiply back
    val *= 3.3;  // Multiply by 3.3V, our reference voltage
    val /= 1024; // convert to voltage
    return val;
}

/**
 * Take the average of 3 samples, not including outliers
 */
float batteryLevelAveraged()
{
    float bat_1, bat_2, bat_3;
    bat_1 = batteryLevel();
    delay(100);
    bat_2 = batteryLevel();
    delay(100);
    bat_3 = batteryLevel();
    float bat_mean = (bat_1 + bat_2 + bat_3) / 3;
    float bat_std = sqrt( 
        (pow((bat_1 - bat_mean), 2) +
        pow((bat_2 - bat_mean), 2) +
        pow((bat_3 - bat_mean), 2)) / 3);
    float total = 0.0;
    int count = 0;
    float std_cutoff = 1.0; // samples within 1 standard deviation expected
    if (abs(bat_1 - bat_mean) <= std_cutoff * bat_std) {
        total += bat_1;
        count++;
    }
    if (abs(bat_2 - bat_mean) <= std_cutoff * bat_std) {
        total += bat_2;
        count++;
    }
    if (abs(bat_3 - bat_mean) <= std_cutoff * bat_std) {
        total += bat_3;
        count++;
    }
    if (count == 0) { // in case nothing passes the standard deviation test
        return (bat_1 + bat_2 + bat_3) / 3;
    }
    return total / count;
}
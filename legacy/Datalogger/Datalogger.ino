#include "SHARP_GP2Y1010AU0F.h"
#include "RTClib.h"
#include <SPI.h>
#include <SD.h>
#include "RTClib.h"
#include <Wire.h>
#include <Adafruit_SleepyDog.h>

#define VBATPIN 9
#define DEFAULT_SD_CHIP_SELECT_PIN 10 //4 for Uno
#define ALTERNATE_RFM95_CS 19 //10 for Uno
#define DEFAULT_RFM95_CS 8 //not needed for Uno

static bool USE_WATCHDOG = true;

/* sensor configs */
//log out clock time -> use GPS
uint8_t SHARP_GP2Y1010AU0F_DUST_PIN = A0; //was A3 for Uno

/* real time clock */
RTC_PCF8523 rtc;
bool setClock = true;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
int nodeId = 1;


float batteryLevel()
{
    float measuredvbat = analogRead(VBATPIN);
    measuredvbat *= 2;    // we divided by 2, so multiply back
    measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
    measuredvbat /= 1024; // convert to voltage
    return measuredvbat;
}

bool init_sd()
{
    digitalWrite(DEFAULT_SD_CHIP_SELECT_PIN, HIGH);
    digitalWrite(DEFAULT_RFM95_CS, HIGH);
    digitalWrite(ALTERNATE_RFM95_CS, HIGH);
    Serial.print("Initializing SD card...");
    if (!SD.begin(DEFAULT_SD_CHIP_SELECT_PIN)) {
        digitalWrite(ALTERNATE_RFM95_CS, LOW);
        digitalWrite(DEFAULT_SD_CHIP_SELECT_PIN, LOW);
        Serial.println("Card failed, or not present");
        return false;
    }
    Serial.println("card initialized.");
    return true;
}

void setup() {
    // Open serial communications and wait for port to open:
    //while (!Serial);
    //Serial.begin(115200);
    if (!init_sd()) { while(1); }

    /* Sharp GP2Y1010AU0F dust */
    if (SHARP_GP2Y1010AU0F::setup(SHARP_GP2Y1010AU0F_DUST_PIN)) { //setup to pin A0
        Serial.println("Setting up sensors...");
    }
    else {
        Serial.println("Dust sensor setup failed");
    }

    /* RTC Clock */
    if (!rtc.begin()) {
        Serial.println("Error: Failed to initialize RTC");
    }
    if (setClock) {
        Serial.print("Printing initial DateTime: ");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        Serial.print(F(__DATE__));
        Serial.print('/');
        Serial.println(F(__TIME__));
    }
}

static long last_data_sample = 0;
void loop() {
    long diff = rtc.now().secondstime() - last_data_sample;
    if (diff < 30) {
        if (USE_WATCHDOG) {
            int sleepMS = Watchdog.sleep(30 - diff); // sleep up to the remainder of the period
        }
        return;
    }
    last_data_sample = rtc.now().secondstime();
    //digitalWrite(LED_BUILTIN, HIGH);
    Serial.print("Printing node number: ");
    Serial.println(nodeId);
    File dataFile = SD.open("datalog.csv", FILE_WRITE);
    if (dataFile) {
        Serial.println("Opening dataFile...");
    } else {
        Serial.println("error opening datalog.csv");
    }
    DateTime now = rtc.now();
    Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.print(" (");
    Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
    Serial.print(") ");
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.println();
    dataFile.print(now.year(), DEC);
    dataFile.print("-");
    dataFile.print(now.month(), DEC);
    dataFile.print("-");
    dataFile.print(now.day(), DEC);
    dataFile.print("T");
    dataFile.print(now.hour(), DEC);
    dataFile.print(":");
    dataFile.print(now.minute(), DEC);
    dataFile.print(":");
    dataFile.print(now.second(), DEC);
    dataFile.print(",");
    dataFile.print(batteryLevel(), DEC);
    dataFile.print(",");
    static int dust_sense_vo_measured = 0;
    static float dust_sense_calc_voltage = 0;
    static float dust_density = 0;
    digitalWrite(DUST_SENSOR_LED_POWER, LOW);
    delayMicroseconds(DUST_SENSOR_SAMPLING_TIME);
    dust_sense_vo_measured = analogRead(SHARP_GP2Y1010AU0F_DUST_PIN);
    delayMicroseconds(DUST_SENSOR_DELTA_TIME);
    digitalWrite(DUST_SENSOR_LED_POWER, DUST_SENSOR_LED_OFF);
    delayMicroseconds(DUST_SENSOR_SLEEP_TIME);
    dust_sense_calc_voltage = dust_sense_vo_measured * (DUST_SENSOR_VCC / 1024);
    dust_density = 0.17 * dust_sense_calc_voltage - 0.1;
    Serial.print(F("Raw Signal Value (0-1023): "));
    Serial.print(dust_sense_vo_measured, DEC);
    dataFile.print(dust_sense_vo_measured, DEC);
    Serial.print(F(" - Voltage: "));
    dataFile.print(",");
    Serial.print(dust_sense_calc_voltage);
    dataFile.print(dust_sense_calc_voltage);
    Serial.print(F(" - Dust Density: "));
    Serial.println(dust_density);
    dataFile.println(dust_density);
    dataFile.close();
    Serial.println("Closing dataFile");
    digitalWrite(LED_BUILTIN, HIGH);
    Watchdog.sleep(500);
    digitalWrite(LED_BUILTIN, LOW);
}


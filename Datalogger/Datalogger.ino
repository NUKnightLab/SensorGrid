#include "SHARP_GP2Y1010AU0F.h"
#include "RTClib.h"

/*
  SD card datalogger
  This example shows how to log data from three analog sensors
  to an SD card using the SD library.
  The circuit:
   analog sensors on analog ins 0, 1, and 2
   SD card attached to SPI bus as follows:
 ** MOSI - pin 11
 ** MISO - pin 12
 ** CLK - pin 13
 ** CS - pin 4 (for MKRZero SD: SDCARD_SS_PIN)
  created  24 Nov 2010
  modified 9 Apr 2012
  by Tom Igoe
  This example code is in the public domain.
*/

#include <SPI.h>
#include <SD.h>
#include "RTClib.h"
#include <Wire.h>

#define DEFAULT_SD_CHIP_SELECT_PIN 4//10
#define ALTERNATE_RFM95_CS 10 //19

/* sensor configs */
//log out clock time -> use GPS
uint8_t SHARP_GP2Y1010AU0F_DUST_PIN = A3;

/* real time clock */
RTC_PCF8523 rtc;
bool setClock = true;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
int nodeId = 1;

void setup() {
  digitalWrite(DEFAULT_SD_CHIP_SELECT_PIN, HIGH);
  //digitalWrite(DEFAULT_RFM95_CS, HIGH);
  digitalWrite(ALTERNATE_RFM95_CS, HIGH);
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }


  Serial.print("Initializing SD card...");

  // see if the card is present and can be initialized:

  if (!SD.begin(DEFAULT_SD_CHIP_SELECT_PIN)) {
    digitalWrite(ALTERNATE_RFM95_CS, LOW);
    digitalWrite(DEFAULT_SD_CHIP_SELECT_PIN, LOW);
    Serial.println("Card failed, or not present");
    // don't do anything more:
    return;
  }
  Serial.println("card initialized.");

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

void loop() {
  Serial.print("Printing node number: ");
  Serial.println(nodeId);
  File dataFile = SD.open("datalog.txt", FILE_WRITE);

  if (dataFile) {
    Serial.println("Opening dataFile...");
  }
  else {
    Serial.println("error opening datalog.txt");
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
    dataFile.print('/');
    dataFile.print(now.month(), DEC);
    dataFile.print('/');
    dataFile.print(now.day(), DEC);
    dataFile.print(" (");
    dataFile.print(daysOfTheWeek[now.dayOfTheWeek()]);
    dataFile.print(") ");
    dataFile.print(now.hour(), DEC);
    dataFile.print(':');
    dataFile.print(now.minute(), DEC);
    dataFile.print(':');
    dataFile.print(now.second(), DEC);
    dataFile.println();  

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
  dataFile.print(F("Raw Signal Value (0-1023): "));
  Serial.print(dust_sense_vo_measured, DEC);
  dataFile.print(dust_sense_vo_measured, DEC);
  Serial.print(F(" - Voltage: "));
  dataFile.print(F(" - Voltage: "));
  Serial.print(dust_sense_calc_voltage);
  dataFile.print(dust_sense_calc_voltage);
  Serial.print(F(" - Dust Density: "));
  dataFile.print(F(" - Dust Density: "));
  Serial.println(dust_density);
  dataFile.println(dust_density);

  dataFile.close();
  Serial.println("Closing dataFile");

  delay(10000);
}


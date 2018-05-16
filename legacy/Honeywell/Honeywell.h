#ifndef _HONEYWELL_H
#define _HONEYWELL_H

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_FeatherOLED.h>
#include <RTClib.h>
#include <KnightLab_GPS.h>

#define VBATPIN 9
#define BUTTON_A 9
#define BUTTON_B 6
#define BUTTON_C 5

extern RTC_PCF8523 rtc;
extern Adafruit_FeatherOLED display;
extern uint32_t display_clock_time;

uint32_t displayCurrentRTCDateTime();
void setDate();
void setupDisplay();
void displayBatteryLevel();
void updateDateTimeDisplay();
void updateGPSDisplay();
void displayMessage(char* message);
void displayID();
void displayTx(int toID);
void displayRx(int fromID, float rssi);
void updateDisplayBattery();
void updateDisplay();
float batteryLevel();


#endif

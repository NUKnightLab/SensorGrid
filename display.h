#ifndef _DISPLAY_H
#define _DISPLAY_H

#include "SensorGrid.h"

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

#endif

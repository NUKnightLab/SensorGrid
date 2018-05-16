#include "display.h"
#include "config.h"

uint8_t lastMinute;

void displayCurrentRTCDate()
{
    DateTime now = rtc.now();
    display.clearMsgArea();
    display.setCursor(0,8);
    display.print(now.month(), DEC); display.print('/');
    display.print(now.day(), DEC); display.print('/');
    display.print(now.year(), DEC);
}

void displayCurrentRTCTime()
{
    display.setCursor(0,16);
    DateTime now = rtc.now();
    display.print(now.hour(), DEC); display.print(':');
    display.print(now.minute(), DEC);
}

void _displayCurrentRTCDateTime()
{
    displayCurrentRTCDate();
    displayCurrentRTCTime();
    lastMinute = rtc.now().minute();   
}

uint32_t displayCurrentRTCDateTime()
{
    DateTime now = rtc.now();
    now = DateTime(now.year(),now.month(),now.day(),now.hour(),now.minute(),0);
    display.setCursor(0,8);
    display.print(now.hour(), DEC); display.print(':');
    if (now.minute() < 10)
        display.print(0, DEC);
    display.print(now.minute(), DEC);  
    display.setCursor(38,8);
    display.print(now.month(), DEC); display.print('/');
    display.print(now.day(), DEC); display.print('/');
    display.print(now.year(), DEC);
    return now.unixtime();
}

void updateDateTimeDisplay()
{
    if (rtc.now().minute() != lastMinute) {
        displayCurrentRTCDateTime();
    }
}

void updateGPSDisplay()
{
      display.setCursor(0,16);
      if (GPS.year == 0) {
          display.print("No GPS");
      } else {
          display.print("fix:");
          if (GPS.fix) {
              display.print("y");
          } else {
              display.print("n");
          }
          display.print(" qual:");
          display.print(GPS.fixquality, DEC);
          display.print(" sats:");
          display.print(GPS.satellites, DEC);
          display.fillRect(0, 24, 128, 29, BLACK);
          display.setCursor(0,24);
          display.print("lt:"); display.print(GPS.latitudeDegrees, 3);
          display.print(" ln:"); display.print(GPS.longitudeDegrees, 3);
      }
}

void _setDate()
{
    DateTime now = rtc.now();
    Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.print(' ');
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.println();
    int yr = 2017;
    int mnth = now.month();
    int date = now.day();
    display.setCursor(0,0);
    display.print("Year: ");
    display.println(yr, DEC);
    display.display();
    display.setCursor(0,8);
    display.print("Month: ");
    display.println(mnth, DEC);
    display.display();
    int startTime = millis();
    while (true) {
      if (! digitalRead(BUTTON_A)) {
          display.setCursor(42,8);
          display.setTextColor(BLACK);
          display.print(mnth++, DEC);
          if (mnth > 12) mnth = 1;
          //display.display();
          display.setCursor(42,8);
          display.setTextColor(WHITE);
          display.print(mnth, DEC);
          display.display();
      }
      if (! digitalRead(BUTTON_C)) break;
      delay(100);
      if (millis() - startTime > 10*1000) break;
      yield();
    }
    display.setCursor(0,16);
    display.print("Date: ");
    display.println(date, DEC);
    display.display();
    startTime = millis();
    while (true) {
      if (! digitalRead(BUTTON_A)) {
          display.setCursor(36, 16);
          display.setTextColor(BLACK);
          display.print(date++, DEC);
          if (date > 31) date = 1;
          display.setCursor(36,16);
          display.setTextColor(WHITE);
          display.print(date, DEC);
          display.display();
      }
      if (! digitalRead(BUTTON_C)) break;
      delay(100);
      if (millis() - startTime > 10*1000) break;
      yield();
    }
    now = rtc.now();
    rtc.adjust(DateTime(yr, mnth, date, now.hour(), now.minute(), now.second()));
    displayCurrentRTCDate();
}

/*
void setDate() {
    display.clearDisplay();
    _setDate();
    display.setCursor(0, 8);
    display.print("OK? (A=Yes / C=No)");
    display.display();
    int startTime = millis();
    while (true) {
        if (! digitalRead(BUTTON_A)) {
            displayCurrentRTCDateTime();
            return;
        }
        if (! digitalRead(BUTTON_C)) return setDate();
        delay(100);
        if (millis() - startTime > 10*1000) break;
        yield();
    }
    display.clearDisplay();
    displayCurrentRTCDateTime();
    display.display();
}
*/

void displayBatteryLevel()
{
    display.setBattery(batteryLevel());
    //display.renderBattery();
    display.display();
}

void displayID()
{
    display.setCursor(0,0);
    display.print("ID: ");
    display.print(config.node_id);
    display.display();
}

void displayMessage(char* message)
{
    display.fillRect(0, 24, 6*21, 24+5, BLACK);
    display.setCursor(0,24);
    display.print(message);
    display.display();
}

void displayTx(int toID)
{
    display.fillRect(0, 24, 45, 29, BLACK);
    display.setCursor(0,24);
    display.print("TX:");
    display.print(toID, DEC);
    display.display();
}

void displayRx(int fromID, float rssi)
{
    display.fillRect(45, 24, 128, 29, BLACK);
    display.setCursor(45, 24);
    display.print("RX:");
    display.print(fromID, DEC);
    display.print(" (");
    display.print(rssi, 2);
    display.print(")");
    display.display();
}

void setupDisplay()
{
    display.init();
    pinMode(BUTTON_A, INPUT_PULLUP); // we may be having conflicts with this button
    pinMode(BUTTON_C, INPUT_PULLUP);
    display.setBatteryIcon(true);
    display.setBattery(batteryLevel());
    display.renderBattery();
    displayCurrentRTCDateTime();
    displayID();
}

float bat = 0.0;

void updateDisplayBattery()
{
     if (batteryLevel() != bat) {
        display.setCursor(45,0);
        display.fillRect(77, 0, 128, 7, BLACK);
        bat = batteryLevel();
        display.setBattery(bat);
        display.renderBattery();
        display.display();         
    }    
}

void updateDisplay()
{
    DateTime now = rtc.now();
    now = DateTime(now.year(),now.month(),now.day(),now.hour(),now.minute(),0);
    if (now.unixtime() != display_clock_time) {
        display.clearMsgArea();
        display_clock_time = displayCurrentRTCDateTime();
        updateGPSDisplay();
        display.display();
    }     
}


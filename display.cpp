#include "display.h"

static RTC_PCF8523 rtc;
uint8_t lastMinute;

void displayCurrentRTCDate() {
    DateTime now = rtc.now();
    display.clearMsgArea();
    display.setCursor(0,8);
    display.print(now.month(), DEC); display.print('/');
    display.print(now.day(), DEC); display.print('/');
    display.print(now.year(), DEC);
}

void displayCurrentRTCTime() {
    display.setCursor(0,16);
    DateTime now = rtc.now();
    display.print(now.hour(), DEC); display.print(':');
    display.print(now.minute(), DEC);
}

void displayCurrentRTCDateTime() {
    displayCurrentRTCDate();
    displayCurrentRTCTime();
    lastMinute = rtc.now().minute();
    display.display();
}

void updateDateTimeDisplay() {
    if (rtc.now().minute() != lastMinute) {
        displayCurrentRTCDateTime();
    }
}

void _setDate() {
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
      //display.display();
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
          //display.display();
          display.setCursor(36,16);
          display.setTextColor(WHITE);
          display.print(date, DEC);
          display.display();
      }
      if (! digitalRead(BUTTON_C)) break;
      delay(100);
      if (millis() - startTime > 10*1000) break;
      yield();
      //display.display();
    }
    now = rtc.now();
    rtc.adjust(DateTime(yr, mnth, date, now.hour(), now.minute(), now.second()));
    displayCurrentRTCDate();
}

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

void displayBatteryLevel() {
    display.setBattery(batteryLevel());
    //display.renderBattery();
    display.display();
}

void setupDisplay() {
    display.init();
    pinMode(BUTTON_A, INPUT_PULLUP); // we may be having conflicts with this button
    pinMode(BUTTON_C, INPUT_PULLUP);
    display.setBatteryIcon(true);
    display.setBattery(batteryLevel());
    display.renderBattery();
    displayCurrentRTCDateTime();
}


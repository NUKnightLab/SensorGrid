#include "oled.h"

static uint8_t last_minute;
char last_datestring[17];

/*
static RTC_PCF8523 _rtc;
static Adafruit_FeatherOLED _oled;
static bool _on;
static uint32_t _activated_time;
static volatile int aButtonState;


void displayDateTimeOLED()
{
    if (!_on) return;
    DateTime now = _rtc.now();
    if (now.minute() == last_minute) return;
    _oled.fillRect(0, 8, 16*6, 7, BLACK);
    _oled.setTextColor(WHITE);
    sprintf(last_datestring, "%d-%02d-%02d %02d:%02d", now.year(), now.month(),
        now.day(), now.hour(), now.minute());
    _oled.setCursor(0,8);
    _oled.print(last_datestring);
    _oled.display();
    last_minute = now.minute();
}

void clearOLED()
{
    _oled.clearDisplay();
    _oled.display();
}

void standbyOLED()
{
    clearOLED();
    _on = false;
}

void aButton_ISR()
{
    aButtonState = !digitalRead(BUTTON_A);
    if (aButtonState) {
        _on = !_on;
        if (_on) {
            _activated_time = millis();
            displayDateTimeOLED();
        } else {
            standbyOLED();
        }
    }
}

void initOLED(RTC_PCF8523 &rtc)
{
    _rtc = rtc;
    _oled.init();
    attachInterrupt(BUTTON_A, aButton_ISR, CHANGE);
    _activated_time = millis();
}
*/

OLED::OLED(RTC_PCF8523 &rtc)
{
    _rtc = rtc;
    Adafruit_FeatherOLED _oled = Adafruit_FeatherOLED();
    _on = false;
    aButtonState = 0;
    //_oled.init();
    //pinMode(BUTTON_A, INPUT_PULLUP); // we may be having conflicts with this button
    //pinMode(BUTTON_C, INPUT_PULLUP);
    //display.setBatteryIcon(true);
    //display.setBattery(batteryLevel());
    //display.renderBattery();
    //displayCurrentRTCDateTime();
    //displayID();
}

void OLED::displayDateTime(bool force_refresh)
{
    if (!_on) return;
    DateTime now = _rtc.now();
    if (force_refresh || now.minute() != last_minute) {
        _oled.fillRect(0, 8, 16*6, 7, BLACK);
        _oled.setTextColor(WHITE);
        sprintf(last_datestring, "%d-%02d-%02d %02d:%02d", now.year(), now.month(),
            now.day(), now.hour(), now.minute());
        _oled.setCursor(0,8);
        _oled.print(last_datestring);
        _oled.display();
        last_minute = now.minute();
    }
}

void OLED::init()
{
    _oled.init();
    clear();
    _on = true;
    //attachInterrupt(BUTTON_A, aButton_ISR, CHANGE);
    _activated_time = millis();
}

void OLED::clear()
{
    _oled.clearDisplay();
    _oled.display();
}

void OLED::standby()
{
    clear();
    _on = false;
}

//void OLED::setButtonFunction(uint32_t pin, void (*fcn)(void), int state)
void OLED::setButtonFunction(uint32_t pin, voidFuncPtr fcn, uint32_t mode)
{
    attachInterrupt(pin, fcn, mode);
}

void OLED::on()
{
    _oled.ssd1306_command(SSD1306_DISPLAYON);
}

void OLED::off()
{
    _oled.ssd1306_command(SSD1306_DISPLAYOFF);
}

void OLED::toggleDisplayState()
{
    _on = !_on;
    if (_on) {
        on();
    } else {
        off();
    }
}

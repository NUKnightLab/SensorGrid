#ifndef SENSORGRIDPM_OLED_H_
#define SENSORGRIDPM_OLED_H_

#include <RTClib.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_FeatherOLED.h>
#define BUTTON_A 9

// extern void initOLED(RTC_PCF8523 &rtc);
// extern void displayDateTimeOLED();
// extern void clear();
// extern void standby();

class OLED {
    RTC_PCF8523 _rtc;
    Adafruit_FeatherOLED _oled;
    bool _on;
    uint32_t _activated_time;
    volatile int aButtonState;

 public:
    OLED(RTC_PCF8523 &rtc);
    void displayDateTime(bool force_refresh = false);
    void init();
    void clear();
    void standby();
    // void setButtonFunction(int button, void (*fcn)(void), int state);
    void setButtonFunction(uint32_t pin, voidFuncPtr fcn, uint32_t mode);
    void on();
    void off();
    bool isOn();
    void toggleDisplayState();
    void displayStartup();
    void endDisplayStartup();
};


#endif  // SENSORGRIDPM_OLED_H_

#include <Arduino.h>
#include <unity.h>

#include "KnightLab_ArduinoUtils/test.h"
#include "KnightLab_FeatherUtils/test.h"
#include "KnightLab_LoRaUtils/test.h"
#include "KnightLab_RecordQueue/test.h"

#include "config.h"


#ifndef LED_BUILTIN
#define LED_BUILTIN 13
#endif


void test_test(void) {
    TEST_ASSERT_EQUAL(1,1);
}

void test_led_state_high(void) {
    digitalWrite(LED_BUILTIN, HIGH);
    //TEST_ASSERT_EQUAL(HIGH, digitalRead(LED_BUILTIN));
}

void test_led_state_low(void) {
    digitalWrite(LED_BUILTIN, LOW);
    TEST_ASSERT_EQUAL(LOW, digitalRead(LED_BUILTIN));
}

void setup() {
    // NOTE!!! Wait for >2 secs
    // if board doesn't support software reset via Serial.DTR/RTS
    //delay(4000);
    while (!Serial);
    set_logging(true);
    UNITY_BEGIN();
    Test_KnightLab_ArduinoUtils::test_all();
    Test_KnightLab_FeatherUtils::test_all();
    //Test_KnightLab_LoRaUtils::test_all();
    Test_KnightLab_RecordQueue::test_all();
    UNITY_END(); // stop unit testing
}

uint8_t i = 0;
uint8_t max_blinks = 5;

void loop() {
    /*
    if (i < max_blinks)
    {
        RUN_TEST(test_led_state_high);
        delay(500);
        RUN_TEST(test_led_state_low);
        delay(500);
        i++;
    }
    else if (i == max_blinks) {
      UNITY_END(); // stop unit testing
    }
    */
}
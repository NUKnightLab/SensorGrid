#ifdef UNIT_TEST

#include <Arduino.h>
#include <unity.h>

#include "test.h"
#include "KnightLab_ArduinoUtils.h"

namespace Test_KnightLab_ArduinoUtils {

    void KnightLab_ArduinoUtils__test_test(void) {
        TEST_ASSERT_EQUAL(1,1);
    }

    void KnightLab_ArduinoUtils__test_ftoa(void) {
        char val[4];
        ftoa(1.23, val, 2);
        TEST_ASSERT_EQUAL_STRING_LEN(val, "1.23", 4);
    }


    /* test runners */

    void test_all() {
        RUN_TEST(KnightLab_ArduinoUtils__test_test);
        RUN_TEST(KnightLab_ArduinoUtils__test_ftoa);
    }
}

#endif
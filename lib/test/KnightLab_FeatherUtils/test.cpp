#ifdef UNIT_TEST

#include <Arduino.h>
#include <unity.h>


namespace Test_KnightLab_FeatherUtils {

    void KnightLab_FeatherUtils__test_test(void) {
        TEST_ASSERT_EQUAL(1,1);
    }

    void test_all() {
        RUN_TEST(KnightLab_FeatherUtils__test_test);
    }

}

#endif
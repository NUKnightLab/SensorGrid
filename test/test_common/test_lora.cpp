#ifdef UNIT_TEST

#include <Arduino.h>
#include <unity.h>

#include "config.h"
#include "lora.h"


namespace Test_SensorGrid_lora {

    void SensorGrid_lora__test_radio_setup(void) {
        uint8_t chip_select = atoi(DEFAULT_RFM95_CS);
        uint8_t interrupt = atoi(DEFAULT_RFM95_INT);
        uint8_t node_id = 1;
        setupRadio(chip_select, interrupt, node_id);
        radio->sleep();
        //TEST_ASSERT_EQUAL(0, router->getRouteTo(1)->dest); 
        TEST_ASSERT_EQUAL(0, router->getRouteTo(2)->dest); 
    }

    void test_all() {
        RUN_TEST(SensorGrid_lora__test_radio_setup);
    }

}

#endif
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
        TEST_ASSERT_EQUAL(1, router->getRouteTo(1)->dest); 
        TEST_ASSERT_EQUAL(2, router->getRouteTo(2)->dest); 
        radio->sleep();
        //TEST_ASSERT_EQUAL(0, router->getRouteTo(1)->dest); 
    }

    void SensorGrid_lora__test_receive(void) {
        uint8_t msg_buf[140] = {0};
        Message *msg = reinterpret_cast<Message*>(msg_buf);
        int resp = receive(msg, 1);
        TEST_ASSERT_EQUAL(RECV_STATUS_NO_MESSAGE, resp);
    }

    void SensorGrid_lora__test_send(void) {
        uint8_t msg_buf[140] = {0};
        Message *msg = reinterpret_cast<Message*>(msg_buf);
        msg->sensorgrid_version = 1;
        msg->network_id = 1;
        msg->from_node = 1;
        msg->message_type = 1;
        msg->len = 12;
        snprintf(&msg->data[0], 100, "TEST MESSAGE");
        uint8_t resp = send_message(msg_buf, 100, 2);
        TEST_ASSERT_EQUAL(RH_ROUTER_ERROR_UNABLE_TO_DELIVER, resp);
    }

    void test_all() {
        RUN_TEST(SensorGrid_lora__test_radio_setup);
        RUN_TEST(SensorGrid_lora__test_send);
        RUN_TEST(SensorGrid_lora__test_receive);
    }

}

#endif
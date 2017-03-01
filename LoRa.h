#include "sensor_grid.h"
#include <RH_RF95.h>

#if defined(__AVR_ATmega32U4__)
    #define RFM95_CS 8
    #define RFM95_RST 4
    #define RFM95_INT 7
#elif defined(ARDUINO_ARCH_SAMD)
    #define RFM95_CS 8
    #define RFM95_RST 4
    #define RFM95_INT 3
#endif

RH_RF95 rf95(RFM95_CS, RFM95_INT);

void setupRadio() {
    Serial.println(F("LoRa Test!"));
    digitalWrite(RFM95_RST, LOW);
    delay(10);
    digitalWrite(RFM95_RST, HIGH);
    delay(10);
    while (!rf95.init()) {
        fail(LORA_INIT_FAIL);
    }
    Serial.println(F("LoRa OK!"));
    if (!rf95.setFrequency(RF95_FREQ)) {
        fail(LORA_FREQ_FAIL);
    }
    Serial.print(F("FREQ: ")); Serial.println(RF95_FREQ);
    rf95.setTxPower(TX_POWER, false);
    delay(100);
}

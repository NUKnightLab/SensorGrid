#include <RH_RF95.h>

/* Feather 32u4 Chipset */
#if BOARD == Feather32u4
    #define RFM95_CS 8
    #define RFM95_RST 4
    #define RFM95_INT 7
#endif

/* Feather M0 Chipset */
#if BOARD == FeatherM0
    #define RFM95_CS 8
    #define RFM95_RST 4
    #define RFM95_INT 3
#endif

RH_RF95 rf95(RFM95_CS, RFM95_INT);

void setupRadio() {
    Serial.println("Feather LoRa RX Test!");
    digitalWrite(RFM95_RST, LOW); Serial.println("LoRa RST LOW");
    delay(10);
    digitalWrite(RFM95_RST, HIGH); Serial.println("LoRa RST HIGHT");
    delay(10);
    while (!rf95.init()) {
        Serial.println("LoRa radio init failed");
        while (1);
    }
    Serial.println("LoRa radio init OK!");
    if (!rf95.setFrequency(RF95_FREQ)) {
        Serial.println("setFrequency failed");
        while (1);
    }
    Serial.print("Set Freq to: "); Serial.println(RF95_FREQ);
    rf95.setTxPower(TX_POWER, false);
    delay(100);
}

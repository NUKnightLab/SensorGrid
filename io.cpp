#include "sensor_grid.h"
#include "io.h"

RH_RF95 rf95(RFM95_CS, RFM95_INT);

void clearMessage() {
    memset(&msgBytes, 0, msgLen);
}

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

void sendCurrentMessage() {
    rf95.send(msgBytes, sizeof(Message));
    rf95.waitPacketSent();
    flashLED(3, HIGH);
}

void printMessageData() {
    Serial.print(F("VER: ")); Serial.print((float)msg->ver_100/100);
    Serial.print(F("; NET: ")); Serial.print(msg->net);
    Serial.print(F("; SND: ")); Serial.print(msg->snd);
    Serial.print(F("; ORIG: ")); Serial.print(msg->orig);
    Serial.print(F("; ID: ")); Serial.println(msg->id);
    Serial.print(F("BAT: ")); Serial.println((float)msg->bat_100/100);
    Serial.print(F("; DT: "));
    Serial.print(msg->year); Serial.print(F("-"));
    Serial.print(msg->month); Serial.print(F("-"));
    Serial.print(msg->day); Serial.print(F("T"));
    Serial.print(msg->hour); Serial.print(F(":"));
    Serial.print(msg->minute); Serial.print(F(":"));
    Serial.println(msg->seconds);
    Serial.print(F("    fix: ")); Serial.print(msg->fix);
    Serial.print(F("; lat: ")); Serial.print((float)msg->lat_1000/1000);
    Serial.print(F("; lon: ")); Serial.print((float)msg->lon_1000/1000);
    Serial.print(F("; sats: ")); Serial.println(msg->sats);
    Serial.print(F("    Temp: ")); Serial.print((float)msg->data[TEMPERATURE_100]/100);
    Serial.print(F("; Humid: ")); Serial.println((float)msg->data[HUMIDITY_100]/100);
    Serial.print(F("    Dust: ")); Serial.println((float)msg->data[DUST_100]/100);
    Serial.print(F("    Vis: ")); Serial.print(msg->data[VISIBLE_LIGHT]);
    Serial.print(F("; IR: ")); Serial.print(msg->data[IR_LIGHT]);
    Serial.print(F("; UV: ")); Serial.println(msg->data[UV_LIGHT]);
}

bool postToAPI() {
      #if WIFI_MODULE
        connectWiFi(WIFI_SSID, WIFI_PASS); // keep-alive. This should not be necessary!
        //WIFI_CLIENT.stop();
        if (WIFI_CLIENT.connect(API_SERVER, API_PORT)) {
            Serial.println(F("API:"));
            Serial.print(F("    CON: "));
            Serial.print(API_SERVER);
            Serial.print(F(":")); Serial.println(API_PORT);
            char* messageChars = (char*)msgBytes;
            WIFI_CLIENT.println("POST /data HTTP/1.1");
            WIFI_CLIENT.print("Host: "); WIFI_CLIENT.println(API_HOST);
            WIFI_CLIENT.println("User-Agent: ArduinoWiFi/1.1");
            WIFI_CLIENT.println("Connection: close");
            WIFI_CLIENT.println("Content-Type: application/x-www-form-urlencoded");
            WIFI_CLIENT.print("Content-Length: "); WIFI_CLIENT.println(msgLen);
            Serial.print(F("Message length is: ")); Serial.println(msgLen);
            WIFI_CLIENT.println();
            WIFI_CLIENT.write(msgBytes, msgLen);
            WIFI_CLIENT.println();
            return true;
        } else {
          Serial.println(F("FAIL: API CON"));
          return false;
        }
    #else
        Serial.println(F("NO API POST: Not an end node"));
        return false;
    #endif
}


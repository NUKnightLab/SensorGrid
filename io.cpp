#include "sensor_grid.h"
#include "io.h"

RH_RF95 rf95(RFM95_CS, RFM95_INT);
static int maxIDs[5] = {0};
static uint32_t MSG_ID = 0;
static uint8_t msgLen = sizeof(Message);

static uint8_t* buf[sizeof(Message)] = {0};
//static struct Message message = (Message)buf;
static struct Message *msg = (struct Message*)buf;
static struct Message message = *msg;
//static struct Message message;
//static uint8_t* buf = (uint8*)message;
static char* charBuf = (char*)buf;


static void clearMessage() {
    //memset(&message, 0, msgLen);
    message = {0};
}

static void clearBuffer() {
    memset(buf, 0, msgLen);
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

static void sendCurrentMessage() {
    //rf95.send(msgBytes, sizeof(Message));
    //uint8_t* buf[sizeof(Message)];
    //clearBuffer();
    //message.data[0] = 100;
    //message.data[1] = 101;
    //message.data[2] = 102;
    //message.data[3] = 103;
    //message.data[4] = 104;
    //message.data[5] = 105;
    //message.data[6] = 106;
    //message.data[7] = 107;
    //message.data[8] = 108;
    //message.data[9] = 109;
    //memcpy(buf, &message, msgLen);
    rf95.send(*buf, msgLen);
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


void transmit() {
      MSG_ID++;
      //uint8_t msg_len = sizeof(Message);
      //struct Message *msg = (struct Message*)msgBytes;
      //Serial.println("CLEAR MSG");
      //clearMessage(msgBytes, msgLen);
      //Serial.print("Set ver: "); Serial.println(VERSION);
      //Serial.println( (uint16_t)(roundf(VERSION * 100)) );
      clearBuffer();
      msg->ver_100 = (uint16_t)(roundf(VERSION * 100));
      //Serial.println("Set net");
      msg->net = NETWORK_ID;
      //Serial.println("Set node");
      msg->snd = NODE_ID;
      //Serial.println("Set orig");
      msg->orig = NODE_ID;
      //Serial.println("Set msg id");
      msg->id = MSG_ID;
      //Serial.println("Set bat");
      msg->bat_100 = (int16_t)(roundf(batteryLevel() * 100));
      //Serial.println("Set hour");
      msg->hour = GPS.hour;
      //Serial.println("Set minute");
      msg->minute = GPS.minute;
      //Serial.println("Set secs");
      msg->seconds = GPS.seconds;
      //Serial.println("Set year");
      msg->year = GPS.year;
      //Serial.println("Set month");
      msg->month = GPS.month;
      //Serial.println("Set day");
      msg->day = GPS.day;
      //Serial.println("Set fix");
      msg->fix = GPS.fix;
      //Serial.println("Set lat");
      msg->lat_1000 = (int32_t)(roundf(GPS.latitudeDegrees * 1000));
      //Serial.println("Set lon");
      msg->lon_1000 = (int32_t)(roundf(GPS.longitudeDegrees * 1000));
      //Serial.println("Set sats");
      msg->sats = GPS.satellites;
      Serial.println("print message data");
      printMessageData();

      if (sensorSi7021Module) {
          Serial.println(F("TEMP/HUMIDITY:"));
          msg->data[TEMPERATURE_100] = (int32_t)(sensorSi7021TempHumidity.readTemperature()*100);
          msg->data[HUMIDITY_100] = (int32_t)(sensorSi7021TempHumidity.readHumidity()*100);
          Serial.print(F("    TEMP: ")); Serial.print(msg->data[TEMPERATURE_100]);
          Serial.print(F("; HUMID: ")); Serial.println(msg->data[HUMIDITY_100]);
      }

      if (sensorSi1145Module) {
          Serial.println(F("Vis/IR/UV:"));
          msg->data[VISIBLE_LIGHT] = (int32_t)sensorSi1145UV.readVisible();
          msg->data[IR_LIGHT] = (int32_t)sensorSi1145UV.readIR();
          msg->data[UV_LIGHT] = (int32_t)sensorSi1145UV.readUV();
          Serial.print(F("    VIS: ")); Serial.print(msg->data[VISIBLE_LIGHT]);
          Serial.print(F("; IR: ")); Serial.print(msg->data[IR_LIGHT]);
          Serial.print(F("; UV: ")); Serial.println(msg->data[UV_LIGHT]);
      }

      #if DUST_SENSOR
          msg->data[DUST_100] = (int32_t)(readDustSensor()*100);
          Serial.print(F("DUST: ")); Serial.println((float)msg->data[DUST_100]/100);
      #endif

      if (!WiFiPresent || !postToAPI(WIFI_SSID, WIFI_PASS, API_SERVER, API_HOST, API_PORT, charBuf, msgLen)) {
          sendCurrentMessage();
          Serial.println(F("!TX"));
      }

      flashLED(3, HIGH);
}


static void _receive() {
    //struct Message *msg = (struct Message*)msgBytes;
    //uint8_t msgLen = sizeof(Message);
    //clearMessage();
    clearBuffer();
    //uint8_t buf[sizeof(Message)];
    if (rf95.recv(*buf, &msgLen)) {
        //memcpy(&message, buf, msgLen);
        Serial.print(F(" ..RX (ver: ")); Serial.print(msg->ver_100);
        Serial.print(F(")"));
        Serial.print(F("    RSSI: ")); // min recommended RSSI: -91
        Serial.println(rf95.lastRssi(), DEC);
        Serial.println(charBuf);
        //Serial.println(buf[1]);
        printMessageData();

        flashLED(1, HIGH);

        if ( msg->orig == NODE_ID ) {
            Serial.print(F("NO-OP. OWN MSG: ")); Serial.println(msg->id);
        } else {
            if (msg->id <= maxIDs[msg->orig]) {
                Serial.print(F("Ignore old Msg: "));
                Serial.print(msg->orig); Serial.print("."); Serial.print(msg->id);
                Serial.print(F("(Current Max: ")); Serial.print(maxIDs[msg->orig]);
                Serial.println(F(")"));
                if (maxIDs[msg->orig] - msg->id > 5) {
                    // Crude handling of node reset will drop some packets but eventually
                    // start again. TODO: A better approach would be for each node to store
                    // its Max ID in EEPROM and get rid of this reset - but EEPROM on
                    // M0 Feather boards is complex (if available at all?). See:
                    // https://forums.adafruit.com/viewtopic.php?f=22&t=88272
                    //
                    Serial.println(F("Reset Max ID. Node: "));
                    Serial.println(msg->orig);
                    maxIDs[msg->orig] = 0;
                }
            } else {
                msg->snd = NODE_ID;
                if (!WiFiPresent || !postToAPI(WIFI_SSID, WIFI_PASS, API_SERVER, API_HOST, API_PORT, charBuf, msgLen)) {
                    delay(1000); // needed for sending radio to receive the bounce
                    Serial.print(F("RETRANSMITTING ..."));
                    Serial.print(F("  snd: ")); Serial.print(msg->snd);
                    Serial.print(F("; orig: ")); Serial.print(msg->orig);
                    sendCurrentMessage();
                    maxIDs[msg->orig] = msg->id;
                    Serial.println(F("  ...RETRANSMITTED"));
                }
            }
        }
    }
}

void receive() {
    // randomized receive cycle to avoid loop sync across nodes
    int delta = 5000 + rand() % 5000;
    Serial.print("rand val: "); Serial.println(delta);
    Serial.print(F("NODE ")); Serial.print(NODE_ID);
    Serial.print(F(" LISTEN: ")); Serial.print(delta);
    Serial.println(F("ms"));
    if (rf95.waitAvailableTimeout(delta)) {
        _receive();
    } else {
        Serial.println(F("  ..NO MSG REC"));
    }
}



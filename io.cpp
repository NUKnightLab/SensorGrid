#include "sensor_grid.h"
#include "io.h"

#include <SPI.h>
//#include <SD.h>
#include <SdFat.h>
static SdFat SD;

#define SD_CHIP_SELECT_PIN 10

RH_RF95 rf95(RFM95_CS, RFM95_INT);
static int maxIDs[5] = {0};
static uint32_t MSG_ID = 0;
static uint8_t msgLen = sizeof(Message);

static uint8_t buf[sizeof(Message)] = {0};
static struct Message *msg = (struct Message*)buf;
static struct Message message = *msg;
static char* charBuf = (char*)buf;


static void clearMessage() {
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
    rf95.send((const uint8_t*)buf, msgLen);
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
    Serial.println(F("Data:"));
    Serial.print(F("    [0] ")); Serial.print(msg->data[0]);
    Serial.print(F("    [1] ")); Serial.print(msg->data[1]);
    Serial.print(F("    [2] ")); Serial.print(msg->data[2]);
    Serial.print(F("    [3] ")); Serial.print(msg->data[3]);
    Serial.print(F("    [4] ")); Serial.print(msg->data[4]);
    Serial.print(F("    [5] ")); Serial.print(msg->data[5]);
    Serial.print(F("    [6] ")); Serial.print(msg->data[6]);
    Serial.print(F("    [7] ")); Serial.print(msg->data[7]);
    Serial.print(F("    [8] ")); Serial.print(msg->data[8]);
    Serial.print(F("    [9] ")); Serial.println(msg->data[9]);
}


static char* logline() {
    char str[80];
    sprintf(str, "%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i",
        msg->ver_100, msg->net, msg->snd, msg->orig, msg->id, msg->bat_100,
        msg->hour, msg->minute, msg->seconds, msg->year, msg->month, msg->day,
        msg->fix, msg->lat_1000, msg->lon_1000, msg->sats,
        msg->data[0], msg->data[1], msg->data[2], msg->data[3], msg->data[4],
        msg->data[5], msg->data[6], msg->data[7], msg->data[8], msg->data[9]);
    return str;
}

void transmit() {
      MSG_ID++;
      clearBuffer();
      msg->ver_100 = (uint16_t)(roundf(VERSION * 100));
      msg->net = NETWORK_ID;
      msg->snd = NODE_ID;
      msg->orig = NODE_ID;
      msg->id = MSG_ID;
      msg->bat_100 = (int16_t)(roundf(batteryLevel() * 100));
      msg->hour = GPS.hour;
      msg->minute = GPS.minute;
      msg->seconds = GPS.seconds;
      msg->year = GPS.year;
      msg->month = GPS.month;
      msg->day = GPS.day;
      msg->fix = GPS.fix;
      msg->lat_1000 = (int32_t)(roundf(GPS.latitudeDegrees * 1000));
      msg->lon_1000 = (int32_t)(roundf(GPS.longitudeDegrees * 1000));
      msg->sats = GPS.satellites;

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
          //msg->data[DUST_100] = (int32_t)(readDustSensor()*100);
          Serial.print(F("DUST: ")); Serial.println((float)msg->data[DUST_100]/100);
      #endif

      printMessageData();

      char* line = logline();
      Serial.print("LOGLINE ("); Serial.print(strlen(line)); Serial.println("):");
      Serial.println(line);
      /*
      if (LOGFILE) {
          writeToSD(LOGFILE, line);
      } */
      
      if (!WiFiPresent || !postToAPI(
            getConfig("WIFI_SSID"), getConfig("WIFI_PASS"), getConfig("API_SERVER"), 
            getConfig("API_HOST"), atoi(getConfig("API_PORT")), charBuf, msgLen)) {
          sendCurrentMessage();
          Serial.println(F("!TX"));
      }

      flashLED(3, HIGH);
}


static void _receive() {
    if (rf95.recv(buf, &msgLen)) {
        Serial.print(F(" ..RX (ver: ")); Serial.print(msg->ver_100);
        Serial.print(F(")"));
        Serial.print(F("    RSSI: ")); // min recommended RSSI: -91
        Serial.println(rf95.lastRssi(), DEC);
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
                if (!WiFiPresent || !postToAPI(
                      getConfig("WIFI_SSID"), getConfig("WIFI_PASS"), getConfig("API_SERVER"), 
                      getConfig("API_HOST"), atoi(getConfig("API_PORT")), charBuf, msgLen)) {
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
    clearBuffer();
    int delta = 5000 + rand() % 5000;
    Serial.print(F("NODE ")); Serial.print(NODE_ID);
    Serial.print(F(" LISTEN: ")); Serial.print(delta);
    Serial.println(F("ms"));
    if (rf95.waitAvailableTimeout(delta)) {
        _receive();
    } else {
        Serial.println(F("  ..NO MSG REC"));
    }
}

void _writeToSD(char* filename, char* str) {
  Serial.print("Init SD card ..");
  if (!SD.begin(10)) {
        Serial.println(" .. SD card init failed!");
        return;
  }
  Serial.println(" .. done");
  File file;
  file = SD.open(filename, O_WRITE|O_APPEND|O_CREAT);
  file.println(str);
  file.close();
}

void writeToSD(char* filename, char* str) {
  digitalWrite(8, HIGH);
  _writeToSD(filename, str);
  digitalWrite(8, LOW);
}
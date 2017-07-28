#include "io.h"

#include <SPI.h>
//#include <SD.h>
#include <SdFat.h>
static SdFat SD;

#define SD_CHIP_SELECT_PIN 10
#define HISTORY_SIZE 30

static RH_RF95 rf95(RFM95_CS, RFM95_INT);
static RHMesh router(rf95, 8);
static uint32_t maxTimestamps[MAX_NETWORK_SIZE] = {0};
static uint32_t MSG_ID = 0;
static uint32_t lastAck = 0;
static uint8_t msgLen = sizeof(Message);

static uint8_t buf[sizeof(Message)] = {0};
static struct Message *msg = (struct Message*)buf;
static struct Message message = *msg;
static struct Message history[HISTORY_SIZE];
static char* charBuf = (char*)buf;

static void clearBuffer() {
    memset(buf, 0, msgLen);
}

void setupRadio() {
    Serial.print("MESSAGE LENGTH"); Serial.println(msgLen, DEC);
    Serial.println(F("LoRa Test!"));
    digitalWrite(RFM95_RST, LOW);
    delay(10);
    digitalWrite(RFM95_RST, HIGH);
    delay(10);
    while (!rf95.init()) {
        fail(LORA_INIT_FAIL);
    }
    Serial.println(F("LoRa OK!"));
    if (!rf95.setFrequency(rf95Freq)) {
        fail(LORA_FREQ_FAIL);
    }
    Serial.print(F("FREQ: ")); Serial.println(rf95Freq);
    rf95.setTxPower(txPower, false);
    for (int i=0; i<HISTORY_SIZE; i++) {
      history[i] = {0};
    }
    if (!router.init())
        Serial.println("Router init failed");
    delay(100);
}

static void printHistory() {
    Serial.print("HISTORY: ");
    for(int i=0; i<HISTORY_SIZE; i++) {
        Serial.print(history[i].orig); Serial.print("."); Serial.print(history[i].id); Serial.print(";");
    }
    Serial.println("");
}

static void sendCurrentMessage() {
  Serial.println("Sending to mesh server");
  // Send a message to a rf22_mesh_server
  // A route to the destination will be automatically discovered.
  if (router.sendtoWait((uint8_t*)buf, msgLen, 1) == RH_ROUTER_ERROR_NONE) {
    // It has been reliably delivered to the next node.
    // Now wait for a reply from the ultimate server
    uint8_t len = sizeof(buf);
    uint8_t from;
    clearBuffer();    
    if (router.recvfromAckTimeout(buf, &len, 3000, &from)) {
      Serial.print("got reply from : 0x"); Serial.print(from, HEX);
      Serial.print(": "); Serial.println((char*)buf);
    } else {
      Serial.println("No reply, is rf22_mesh_server1, rf22_mesh_server2 and rf22_mesh_server3 running?");
    }
  } else {
     Serial.println("sendtoWait failed. Are the intermediate mesh servers running?");
  }
  router.printRoutingTable();
}

static void old_sendCurrentMessage() {
    rf95.send((const uint8_t*)buf, msgLen);
    rf95.waitPacketSent();
    flashLED(3, HIGH);
    bool inHistory = false;
    int emptyIndex = -1;
    uint32_t oldestTimestamp = 0;
    int oldestIndex = -1;

    for (int i=0; i<HISTORY_SIZE; i++) {
        if (history[i].orig !=0
            && (oldestTimestamp == 0 || history[i].timestamp < oldestTimestamp) ) {
              oldestTimestamp = history[i].timestamp;
              oldestIndex = i;
        }
    }
    for (int i=0; i<HISTORY_SIZE; i++) {
        if (history[i].orig == 0) {
            emptyIndex = i;
        } else if (oldestTimestamp == 0 || history[i].timestamp < oldestTimestamp) {
            oldestTimestamp = history[i].timestamp;
            oldestIndex = i;
        }
        if (history[i].orig == msg->orig && history[i].id == msg->id) {
            inHistory = true;
        }
        //if (history[i].orig == 0) {
        //    Serial.print("set hist: "); Serial.print(i); Serial.print("To orig: "); Serial.println(msg->orig);
        //    memcpy(&history[i], buf, msgLen);
        //    break;
        //}
    }
    if (!inHistory) {
        if (emptyIndex < 0) {
            Serial.println("WARNING: FULL HISTORY QUEUE!!!"); // TODO: manage full history case
            if (oldestIndex >= 0) {
                Serial.print("DROPPING OLDEST HISTORICAL MESSAGE: ");
                Serial.print(history[oldestIndex].orig); Serial.print("."); Serial.println(history[oldestIndex].id);
                history[oldestIndex] = {0};
                memcpy(&history[oldestIndex], buf, msgLen);
            } else {
                Serial.println("ERROR: Invalid state. Oldest historical index not found!!!");
            }
        }  else {
            memcpy(&history[emptyIndex], buf, msgLen);
        }
    }
    //printHistory();
}

void printMessageData() {
    Serial.print(F("VER: ")); Serial.print((float)msg->ver_100/100);
    Serial.print(F("; NET: ")); Serial.print(msg->net);
    Serial.print(F("; SND: ")); Serial.print(msg->snd);
    Serial.print(F("; ORIG: ")); Serial.print(msg->orig);
    Serial.print(F("; ID: ")); Serial.println(msg->id);
    Serial.print(F("BAT: ")); Serial.println((float)msg->bat_100/100);
    Serial.print(F("TS: ")); Serial.println(msg->timestamp);
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
    char str[200]; // 155+16 is current theoretical max
    sprintf(str,
        "%4.2f,%i,%i,%i,%i,%3.2f,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i",
        (float)(msg->ver_100)/100, msg->net, msg->snd, msg->orig, msg->id, (float)(msg->bat_100)/100, msg->timestamp,
        msg->data[0], msg->data[1], msg->data[2], msg->data[3], msg->data[4],
        msg->data[5], msg->data[6], msg->data[7], msg->data[8], msg->data[9]);
    return str;
}

static void writeLogLine() {
    char* line = logline();
    Serial.print(F("LOGLINE (")); Serial.print(strlen(line)); Serial.println("):");
    Serial.println(line);
    if (logfile) {
        digitalWrite(SD_CHIP_SELECT_PIN, LOW);
        writeToSD(logfile, line);
        digitalWrite(SD_CHIP_SELECT_PIN, HIGH);
    }
}

static void warnNoGPSConfig() {
    Serial.println(F("WARNING! GPS data specified in data registers, but no GPS_MODULE in config"));
}

static uint32_t getDataByTypeName(char* type) {
    Serial.print("Getting data for type: "); Serial.println(type);
    if (!strcmp(type, "GPS_FIX")) {
        if (!gpsModule) warnNoGPSConfig();
        return GPS.fix;
    }
    if (!strcmp(type, "GPS_SATS")) {
        if (!gpsModule) warnNoGPSConfig();
        return GPS.satellites;
    }
    if (!strcmp(type, "GPS_SATFIX")) {
        if (!gpsModule) warnNoGPSConfig();
        if (GPS.fix) return GPS.satellites;
        return -1 * GPS.satellites;
    }
    if (!strcmp(type, "GPS_LAT_DEG")){
        if (!gpsModule) warnNoGPSConfig();
        Serial.println(GPS.lastNMEA());
        Serial.print("LATITUDE "); Serial.println(GPS.latitudeDegrees, DEC);
        return (int32_t)(roundf(GPS.latitudeDegrees * 1000));
    }
    if (!strcmp(type, "GPS_LON_DEG")) {
        if (!gpsModule) warnNoGPSConfig();
        Serial.println(GPS.lastNMEA()); // These are looking pretty ugly. Looks like a bad
                                        // overwrite of NMEA is corrupting the string from
                                        // around the end of the longitude field on GPGGA strings
        Serial.print("LONGITUDE "); Serial.println(GPS.longitudeDegrees, DEC);
        return (int32_t)(roundf(GPS.longitudeDegrees * 1000));
    }
    if (!strcmp(type, "SI7021_TEMP")) {
        return (int32_t)(sensorSi7021TempHumidity.readTemperature()*100);
    }
    if (!strcmp(type, "SI7021_HUMIDITY")) {
        return (int32_t)(sensorSi7021TempHumidity.readHumidity()*100);
    }
    if (!strcmp(type, "SI1145_VIS")) {
        return (int32_t)sensorSi1145UV.readVisible();
    }
    if (!strcmp(type, "SI1145_IR")) {
        return (int32_t)sensorSi1145UV.readIR();
    }
    if (!strcmp(type, "SI1145_UV")) {
        return (int32_t)sensorSi1145UV.readUV();
    }
    if (!strcmp(type, "SHARP_GP2Y1010AU0F_DUST")) {
        return (int32_t)(readDustSensor()*100);
    }
    if (!strcmp(type, "GROVE_AIR_QUALITY_1_3")) {
        return (int32_t)(readGroveAirQualitySensor()*100);
    }
    if (!strcmp(type, "FAKE_3")) {
        return 3333333;
    }
    if (!strcmp(type, "FAKE_4")) {
        return 4444444;
    }
    if (!strcmp(type, "FAKE_5")) {
        return 5555555;
    }
    if (!strcmp(type, "FAKE_6")) {
        return 6666666;
    }
    if (!strcmp(type, "FAKE_7")) {
        return 7777777;
    }
    if (!strcmp(type, "FAKE_8")) {
        return 8888888;
    }
    if (!strcmp(type, "FAKE_9")) {
        return 9999999;
    }
    Serial.print(F("WARNING! Unknown named data type: ")); Serial.println(type);
    return 0;
}

static uint32_t getRegisterData(char* registerName, char* defaultType) {
    char* type = getConfig(registerName);
    if (!type) {
        type = defaultType;
    }
    return getDataByTypeName(type);
}

static uint32_t getRegisterData(char* registerName) {
    char* type = getConfig(registerName);
    if (type) {
        return getDataByTypeName(type);
    } else {
        Serial.print(F("Data register ")); Serial.print(registerName);
        Serial.println(F(" not configured"));
        return 0;
    }
}

static void fillCurrentMessageData() {
      clearBuffer();
      uint16_t ver = (uint16_t)(roundf(protocolVersion * 100));
      msg->ver_100 = ver;
      msg->net = networkID;
      msg->snd = nodeID;
      msg->orig = nodeID;
      msg->id = MSG_ID;
      msg->bat_100 = (int16_t)(roundf(batteryLevel() * 100));
      msg->timestamp = rtc.now().unixtime();
      memcpy(msg->data, {0}, sizeof(msg->data));

      if (gpsModule) {
          /* If GPS_MODULE is set in config file, these data will default to the first
           *  3 data registers, but other configs can be set in the config file.
           */
          msg->data[0] = getRegisterData("DATA_0", "GPS_SATFIX");
          msg->data[1] = getRegisterData("DATA_1", "GPS_LAT_DEG");
          msg->data[2] = getRegisterData("DATA_2", "GPS_LON_DEG");
      } else {
          msg->data[0] = getRegisterData("DATA_0");
          msg->data[1] = getRegisterData("DATA_1");
          msg->data[2] = getRegisterData("DATA_2");
      }
      msg->data[3] = getRegisterData("DATA_3");
      msg->data[4] = getRegisterData("DATA_4");
      msg->data[5] = getRegisterData("DATA_5");
      msg->data[6] = getRegisterData("DATA_6");
      msg->data[7] = getRegisterData("DATA_7");
      msg->data[8] = getRegisterData("DATA_8");
      msg->data[9] = getRegisterData("DATA_9");

}

void reTransmitOldestHistory() {
    uint32_t oldestTimestamp = 0;
    int idx = -1;
    Serial.println("************ RE-TX OLDEST IN HISTORY **********");    
    for (int i=0; i<HISTORY_SIZE; i++) {
        if (history[i].orig !=0
            && (oldestTimestamp == 0 || history[i].timestamp < oldestTimestamp) ) {
              oldestTimestamp = history[i].timestamp;
              idx = i;
        }
    }
    if (idx >= 0) {
        Serial.print("Re-transmit: "); Serial.print(history[idx].orig); Serial.print("."); Serial.println(history[idx].id);
        clearBuffer();
        memcpy(buf, &history[idx], msgLen);
        printMessageData();
        sendCurrentMessage();
    } else {
        Serial.println("Empty history. Nothing to re-transmit");
    }
}

void transmit() {
    for (int i=0; i<HISTORY_SIZE; i++) {
        if (history[i].orig == nodeID && history[i].id == MSG_ID) {
            // Do not transmit new message from this node until
            // we rx an ack for previous message. TODO: prevent black hole of infinitely repeating same message

            // instead of preventing new messages, maybe keep track of historical ids rxed rather than just max timestamp
            Serial.print("Preventing new messages until ACK is received for ID: "); Serial.println(MSG_ID);
            memcpy(buf, &history[i], msgLen);
            printMessageData();
            sendCurrentMessage();
            return;
        }
    }
    MSG_ID++;
    fillCurrentMessageData();
    printMessageData();

    /**
     * Unable to get WINC1500 and Adalogger to share SPI bus for logger writes.
     * WiFi does not want to reconnect after losing the SPI. Things tried:
     *
     * Setting Chip select pin modes (WiFi CS HIGH during logger write, logger LOW with
     *     reset to WiFi LOW after write)
     * WIFI_CLIENT.stop() ... begin()
     * WiFi.end()
     * WiFi.refresh()
     * SPI.endTransaction()
     * SPI.end() .. begin()
     * various time delays
     *
     * In the end, unable to get WiFi to continue after writing to the SD card.
     * For now settle with incompatibility between logging and WiFi API posts.
     * SD card read for configs still seems to be ok.
     *
     * This issue is logged as: https://github.com/NUKnightLab/SensorGrid/issues/2
     */

    if ( !strcmp(logMode, "NODE")
         || !strcmp(logMode, "NETWORK")
         || !strcmp(logMode, "ALL") ) {
       writeLogLine();
    }

    if (!WiFiPresent || !postToAPI(
          getConfig("WIFI_SSID"), getConfig("WIFI_PASS"), getConfig("API_SERVER"),
          getConfig("API_HOST"), atoi(getConfig("API_PORT")),
          charBuf, msgLen)) {
        flashLED(3, HIGH);
        Serial.println("Sending current message");
        sendCurrentMessage();
    }
}

static void _receive() {
  uint8_t from;
  if (router.recvfromAck(buf, &msgLen, &from))
  {
    Serial.print("got request from : 0x");
    Serial.print(from, HEX);
    Serial.print(": ");
    Serial.println((char*)buf);
    // Send a reply back to the originator client
    if (router.sendtoWait((uint8_t*)buf, msgLen, from) != RH_ROUTER_ERROR_NONE)
      Serial.println("sendtoWait failed");
  }
}

static void _orig_receive() {
    if (rf95.recv(buf, &msgLen)) {
        Serial.print(F(" ..RX (ver: ")); Serial.print(msg->ver_100);
        Serial.print(F(")"));
        Serial.print(F("    RSSI: ")); // min recommended RSSI: -91
        Serial.println(rf95.lastRssi(), DEC);
        uint16_t ver = (uint16_t)(roundf(protocolVersion * 100));
        if (msg->ver_100 != ver) {
            Serial.print(F("SKIP: unknown protocol version: ")); Serial.print(msg->ver_100/100);
            return;
        }
        if ( msg->orig != nodeID && (
             (!strcmp(logMode, "NETWORK") && msg->net == networkID)
             || !strcmp(logMode, "ALL") )) {
           writeLogLine();
        }
        if (msg->net > 0 && msg->net != networkID) {
            Serial.println(F("SKIP: out-of-network msg"));
            return;
        }
        if ( msg->orig == nodeID) {
            Serial.print(F("SKIP: own msg from: ")); Serial.println(msg->snd);
            // remove it from history
            for (int i=0; i<HISTORY_SIZE; i++) {
              if (history[i].orig == msg->orig && history[i].id == msg->id) {
                Serial.print("delete from hist: "); Serial.print(msg->orig); Serial.println(msg->id);
                history[i] = {0};
                break;
              }
            }
            printHistory();
            //Serial.print(F("LOST ACK: ")); Serial.println(msg->id - (lastAck+1));
            //lastAck = msg->id;
            return;
        }
        printMessageData();
        flashLED(1, HIGH);
        if (oledOn) {
            display.fillRect(35, 16, 128, 32, BLACK);
            display.display();
            display.setTextColor(WHITE);
            display.setCursor(35, 16);
            display.print(msg->snd); display.print(":");
            display.print(msg->orig); display.print(".");
            display.print(msg->id);
            display.print(" ");display.print(rf95.lastRssi());
            display.display();
        }
        if ( (msg->orig != msg->snd) && (msg->timestamp <= maxTimestamps[msg->orig]) ) {
            /**
             * Always re-transmit 1st hop messages. Retransmit subsequent hops if they are new
             */
            Serial.print(F("Ignore old Msg: "));
            Serial.print(msg->orig); Serial.print("."); Serial.print(msg->id);
            Serial.print(F(" (Current Max: ")); Serial.print(maxTimestamps[msg->orig]);
            Serial.println(F(")"));
            /* Not sure we should be clearing this from history. A second rx does not necessarily mean
             * that we have had a successful tx. E.g., this could be a re-tx from orig that never
             * received an ack
            for (int i=0; i<HISTORY_SIZE; i++) {
              if (history[i].orig == msg->orig && history[i].id == msg->id) {
                Serial.print("delete from hist: "); Serial.print(msg->orig); Serial.println(msg->id);
                history[i] = {0};
                break;
              }
            }
            */
            printHistory();
        } else {
            msg->snd = nodeID;
            if (!WiFiPresent || !postToAPI(
                  getConfig("WIFI_SSID"), getConfig("WIFI_PASS"), getConfig("API_SERVER"),
                  getConfig("API_HOST"), atoi(getConfig("API_PORT")),
                  charBuf, msgLen)) {
                delay(RETRANSMIT_DELAY);
                Serial.print(F("RETRANSMITTING ..."));
                Serial.print(F("  snd: ")); Serial.print(msg->snd);
                Serial.print(F("; orig: ")); Serial.print(msg->orig);
                sendCurrentMessage();
                maxTimestamps[msg->orig] = msg->id;
                Serial.println(F("  ...RETRANSMITTED"));
            }
        }
    }
}

void receive() {
    /**
     * Randomized receive cycle to avoid loop sync across nodes
     */
    clearBuffer();
    int delta = 1000 + rand() % 1000;
    //Serial.print(F("NODE ")); Serial.print(nodeID);
    //Serial.print(F(" LISTEN: ")); Serial.print(delta);
    //Serial.println(F("ms"));
    _receive();
    /*
    if (rf95.waitAvailableTimeout(delta)) {
        _receive();
    } else {
        Serial.println(F("  ..NO MSG REC"));
    } */
}

void _writeToSD(char* filename, char* str) {
  Serial.print(F("Init SD card .."));
  if (!SD.begin(10)) {
        Serial.println(F(" .. SD card init failed!"));
        return;
  }
  if (false) {  // true to check available SD filespace
      Serial.print("SD free: ");
      uint32_t volFree = SD.vol()->freeClusterCount();
      float fs = 0.000512*volFree*SD.vol()->blocksPerCluster();
      Serial.println(fs);
  }
  File file;
  Serial.print(F("Writing to ")); Serial.print(filename);
  Serial.println(F(":")); Serial.println(str);
  file = SD.open(filename, O_WRITE|O_APPEND|O_CREAT);
  file.println(str);
  file.close();
  Serial.println("File closed");
}

void writeToSD(char* filename, char* str) {
  digitalWrite(8, HIGH);
  _writeToSD(filename, str);
  digitalWrite(8, LOW);
}

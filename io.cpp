#include "io.h"

#include <SPI.h>
//#include <SD.h>
#include <SdFat.h>
static SdFat SD;

#define SD_CHIP_SELECT_PIN 10
#define RH_MESH_MAX_MESSAGE_LEN 60

#define ROUTER_TYPE 1

static RH_RF95 rf95(RFM95_CS, RFM95_INT);
#if ROUTER_TYPE == 0
static RHRouter* router;
#else
static RHMesh* router;
#endif

static uint32_t maxTimestamps[MAX_NETWORK_SIZE] = {0};
static uint32_t MSG_ID = 0;
static uint32_t lastAck = 0;
static uint8_t msgLen = sizeof(Message);

static uint8_t buf[sizeof(Message)] = {0};
static uint8_t ackBuffer[50] = {0};
static struct Message *msg = (struct Message*)buf;
static struct Message message = *msg;
static char* charBuf = (char*)buf;
uint8_t data[] = "Hello back from server";
uint8_t routingBuf[RH_MESH_MAX_MESSAGE_LEN];

static void clearBuffer() {
    memset(buf, 0, msgLen);
}

static void clearAckBuffer() {
    memset(ackBuffer, 0, 50);
}

void setupRadio() {
    #if ROUTER_TYPE == 0
        router = new RHRouter(rf95, nodeID);
    #else
        router = new RHMesh(rf95, nodeID);
    #endif
    
    rf95.setModemConfig(RH_RF95::Bw125Cr48Sf4096);
    if (!router->init())
        Serial.println("Router init failed");
        
    if (!rf95.setFrequency(rf95Freq)) {
        fail(LORA_FREQ_FAIL);
    }
    Serial.print(F("FREQ: ")); Serial.println(rf95Freq);
    rf95.setTxPower(txPower, false);
    rf95.setCADTimeout(2000);
    router->setTimeout(1000);

    if (ROUTER_TYPE == 0) {
        router->addRouteTo(1, 108);
        router->addRouteTo(107, 108);
        router->addRouteTo(108, 108);
    }
    delay(100);
}

bool channelActive() {
  return rf95.isChannelActive();
}

static bool sendCurrentMessage() {
    Serial.println("Sending to mesh server");
    clearAckBuffer();
    uint8_t len = sizeof(ackBuffer);
    uint8_t from;
    int errCode;
    errCode = router->sendtoWait((uint8_t*)buf, msgLen, collectorID);
    if (errCode == RH_ROUTER_ERROR_NONE) {
        // It has been reliably delivered to the next node.
        // Now wait for a reply from the ultimate server
        if (router->recvfromAckTimeout(ackBuffer, &len, 5000, &from)) {
        Serial.print("got reply from : 0x"); Serial.print(from, HEX);
        Serial.print(": "); Serial.print((char*)buf);
        Serial.print("; rssi: "); Serial.println(rf95.lastRssi());
      } else {
        Serial.println("recvfromAckTimeout: No reply, is collector running?");
      }
    } else {
       Serial.println("sendtoWait failed. Are the intermediate nodes running?");
    }
    router->printRoutingTable();
    if (errCode == RH_ROUTER_ERROR_NONE) {
      return true;
    } else {
      return false;
    }
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
        //Serial.print(F("Data register ")); Serial.print(registerName);
        //Serial.println(F(" not configured"));
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

bool transmit() {
    fillCurrentMessageData();
    printMessageData();
    return sendCurrentMessage();
}

    /**
     * Note from old transmit function:
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

void receive() {
  uint8_t len = sizeof(buf);
  uint8_t from;
  if (router->recvfromAckTimeout(buf, &len, 5000, &from)) {
    if (nodeID == 1) {
        Serial.print("got request from :");
        Serial.println(from, DEC);
        printMessageData();
    } 
    if (router->sendtoWait(data, sizeof(data), from) != RH_ROUTER_ERROR_NONE)
        Serial.println("sendtoWait failed");
  } else {
    //Serial.println("Got nothing");
  }
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

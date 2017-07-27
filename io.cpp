#include "io.h"

#include <SPI.h>
//#include <SD.h>
#include <SdFat.h>
static SdFat SD;

#define SD_CHIP_SELECT_PIN 10
#define HISTORY_SIZE 30

static RH_RF95 rf95(RFM95_CS, RFM95_INT);
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
void setupRadio(uint8_t nodeID) {
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
    rf95.setThisAddress(nodeID);
    rf95.setHeaderFrom(nodeID);
    for (int i=0; i<HISTORY_SIZE; i++) {
      history[i] = {0};
    }
    delay(100);
}

static void printHistory() {
    Serial.print("HISTORY: ");
    for(int i=0; i<HISTORY_SIZE; i++) {
        Serial.print(history[i].orig); Serial.print("."); Serial.print(history[i].id); Serial.print(";");
    }
    Serial.println("");
}

void sendBeacon() {
    clearBuffer();
    uint16_t ver = (uint16_t)(333); // for now: using version field as message type. 333 = beacon
    msg->ver_100 = ver;
    int32_t ext;
    if (sinkNode) {
        ext = 0;
    } else if (parent == NULL) {
        ext = -1;
    } else {
        ext = parent->ext + 1;
    }
    msg->data[0] = ext;
    msg->data[1] = txPower;
    Serial.println("Sending BEACON");
    rf95.setHeaderTo(255);
    rf95.send((const uint8_t*)buf, msgLen);
    rf95.waitPacketSent();
    Serial.println("Sent BEACON");
}

static void removeParent(uint8_t id) {
    if (parent == NULL) return;
    Serial.print("Removing a parent node: "); Serial.println(id);
    if (parent->id == id) {
        Serial.println("parent ID is id");
        parent = parent->nextNode;
        Serial.println("Parent is now next node");
        // TODO: destroy old parent?
        numParents -= 1;
        return;   
    }
    Node * current = parent;
    while (current->nextNode != NULL) {
        Serial.print("Checking id of next node: "); Serial.println(current->nextNode->id);
        if (current->nextNode->id == id) {
            current->nextNode = current->nextNode->nextNode;
            // TODO: destroy removed node?
            numParents -= 1;
            return;
        }
        current = current->nextNode;
    }
    Serial.println("Done remove");
}

static int compareNodes(Node* n1, Node* n2) {
    if ( n1->ext < n2->ext ) return -1;
    if ( n1->ext > n2->ext ) return 1;
    if ( n1->rssi == n2->rssi && n1->tx == n2->tx) return 0;
    if ( n1->rssi/n1->tx < n2->rssi/n2->tx) return -1;
    if ( n1->rssi/n1->tx > n2->rssi/n2->tx) return 1;
    return 0;
}

static void addParent(Node * node) {
    Serial.println("Adding a parent node");
    // since order is maintained and could have changed, remove and re-add
    removeParent(node->id);
    if (node->ext < 0) {
        Serial.print("Node "); Serial.print(node->id); Serial.println(" is un-routed. Not adding to parents");
        return;
    }
    
    if (parent == NULL) {
        Serial.println("Adding first parent");
        parent = node;
        numParents += 1;
        return;
    }

    Serial.println("Remove-check done. parent is not null");
    Serial.print("parent ext: "); Serial.println(parent->ext);
    Serial.print("parent rssi: "); Serial.print(parent->rssi);
    Serial.print("Node ext: "); Serial.print(node->ext);
    Serial.print("Node rssi: "); Serial.println(node->rssi);
    //if ( (node->ext < parent->ext)
    //      || ( node->ext == parent->ext && node->rssi > parent->rssi )) {
    if ( compareNodes(node, parent) <= 0) {
        node->nextNode = parent;
        parent = node;
        Serial.println("Pushed node in front of parent");
        Serial.println("incrementing numParents");
        numParents += 1;
        Serial.print("number of parents: "); Serial.println(numParents, DEC);
        return;
    }
    Serial.println("New node is greater than current parent");
    Node * current = parent;
    while (current->nextNode != NULL) {
        Serial.print("node ext: "); Serial.println(node->ext);
        Serial.print("next node ext: "); Serial.println(current->nextNode->ext);
        //if (node->ext < current->nextNode->ext
        //  || ( node->ext == current->ext && node->rssi < current->rssi )) {
        if ( compareNodes(node, current->nextNode) <= 0 ) {
            node->nextNode = current->nextNode;
            current->nextNode = node;
            Serial.println("Inserted node into parents");
            numParents += 1;
            Serial.print("number of parents: "); Serial.println(numParents);
            return;
        }
        current = current->nextNode;
    }
    Serial.println("Node goes at end of parent list");
    current->nextNode = node;
    Serial.println("Increment num parents");
    numParents += 1;
    Serial.print("Number of parents"); Serial.println(numParents);
}

void expireParents() {
     Serial.println("Checking for expired parents");
     Node * node = parent;
     while (node != NULL) {
        if ( (millis() - node->timestamp) > 40 * 3000 ) {
            Serial.print("Removing expired parent: "); Serial.println(node->id);
            removeParent(node->id);
        }
        node = node->nextNode;
     }
}

static void handleBeacon() {
    Serial.print("Received beacon from: "); Serial.print(rf95.headerFrom(), DEC);
    Serial.print("; EXT: "); Serial.print(msg->data[0], DEC);
    Serial.print("; TX: "); Serial.print(msg->data[1], DEC);
    Serial.print("; RSSI: "); Serial.println(rf95.lastRssi(), DEC);

    if (sinkNode) {
        Serial.println("Collector node: skip beacon handling");
        return;
    }


    // Not working
    //if (parent != NULL && rf95.headerFrom() != parent->id && msg->data[0] >= parent->ext) {
    //    /* TODO: consider alternative methods to preventing circular routes
    //     *  
    //     * This should help prevent local circular routes but introduces latency/dropped packets when parent is lost
    //     */
    //    Serial.print("Skipping potential parent not closer to sink. ID: "); Serial.println(rf95.headerFrom());
    //    return;
    //}
    
    /*
    if (msg->data[0] < 0) {
        // skip disconnected nodes for now. Later these might inform how we handle beaconing and route construction
        Serial.println("Skipping handling disconnected node beacon");
        return;
    } */
    
    /*
    if (parent == NULL) {
        Serial.println("Creating parent");
        parent = (Node*) malloc(sizeof(Node));
        //memset(parent, 0, sizeof(Node));
        Serial.print("Parent is null: "); Serial.println(parent == NULL);
        parent->id = rf95.headerFrom();
        parent->ext = msg->data[0];
        parent->rssi = rf95.lastRssi();
        Serial.print("Parent is now null: "); Serial.println(parent == NULL);
        Serial.print("Next parent is null: "); Serial.println(parent->nextNode == NULL);
        hopDistanceToSink = parent->ext + 1;
        numParents = 1;
        return;
    } */
    Serial.println("malloc new node");
    Node * node = (Node*) malloc(sizeof(Node));
    node->id = rf95.headerFrom();
    node->timestamp = millis();
    node->ext = msg->data[0];
    node->tx = msg->data[1];
    node->rssi = rf95.lastRssi();
    node->nextNode = NULL;
    addParent(node);
}

static void sendCurrentMessage() {
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

static void _receive() {
    if (rf95.recv(buf, &msgLen)) {
        Serial.print(F(" ..RX (ver: ")); Serial.print(msg->ver_100);
        Serial.print(F(")"));
        Serial.print(F("    RSSI: ")); // min recommended RSSI: -91
        Serial.println(rf95.lastRssi(), DEC);
        uint16_t ver = (uint16_t)(roundf(protocolVersion * 100));
        // temporarily using version 333 to indicate beacon transmission
        if (msg->ver_100 == 333) {
            handleBeacon();
            return;
        }
        if (msg->ver_100 == 444) { // 444: temp data ack code. handle in transmission
            return;
        }
        if (msg->ver_100 != ver) {
            Serial.print(F("SKIP: unknown protocol version: ")); Serial.println(msg->ver_100, DEC);
            return;
        }

        // looks like data packet. send ack
        Serial.print("Received data from: "); Serial.println(rf95.headerFrom());
        delay(500);
        rf95.setHeaderTo(rf95.headerFrom());
        clearBuffer();
        msg->ver_100 = (uint16_t)(444); // for now: using version field as message type. 444 = data ack
        rf95.send((const uint8_t*)buf, msgLen);
        rf95.waitPacketSent();
        Serial.println("Sent ACK");
        return;

        // TODO: forward data to parent
        
        // old _receive
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
    Serial.print(F("NODE ")); Serial.print(nodeID);
    Serial.print(F(" LISTEN: ")); Serial.print(delta);
    Serial.println(F("ms"));
    if (rf95.waitAvailableTimeout(delta)) {
        _receive();
    } else {
        Serial.println(F("  ..NO MSG REC"));
    }
}


static void sendCurrentMessageWithAck() {
    if (parent == NULL) {
        Serial.println("No parent to receive message. Not sending message");
        return;
    }
    bool success = false;
    for (int i=0; i<3; i++) {    
        fillCurrentMessageData();
        printMessageData();
        /* TODO: handle logging:
        if ( !strcmp(logMode, "NODE")
             || !strcmp(logMode, "NETWORK")
             || !strcmp(logMode, "ALL") ) {
           writeLogLine();
        } */
        flashLED(3, HIGH);
        Serial.print("Sending current msg to parent with ID: "); Serial.print(parent->id);
        rf95.setHeaderTo(parent->id);
        rf95.send((const uint8_t*)buf, msgLen);
        rf95.waitPacketSent();
        clearBuffer(); // TODO: we should have separate input and output buffers
        if (rf95.waitAvailableTimeout(1000)) {
            if (rf95.recv(buf, &msgLen)) {
                Serial.print(F(" ..RX (ver: ")); Serial.print(msg->ver_100);
                Serial.print(F(")"));
                Serial.print(F("    RSSI: ")); // min recommended RSSI: -91
                Serial.println(rf95.lastRssi(), DEC);
                if (msg->ver_100 == 333) {
                    // ignore beacons for now
                    Serial.print("Ignoring beacon from: "); Serial.println(rf95.headerFrom());
                    i--;
                    continue;
                } else if (msg->ver_100 == 444) {
                    Serial.println("Received ack");
                    success = true;
                    break;
                } else {
                    Serial.print("Received non-ack. Possible data to handle");
                }
            } else {
                Serial.println(" ..NO MSG RCVD");
            }
        } else {
            Serial.println(F("  ..ACK WAIT TIMEOUT"));
        }
    }
    if (!success) {
        Serial.println("Did not receive ACK from parent. Trying new parent");
        removeParent(parent->id);
        sendCurrentMessageWithAck();
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

void transmitCurrentData() {
    MSG_ID++;
    sendCurrentMessageWithAck();
    // TODO: handle sink
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

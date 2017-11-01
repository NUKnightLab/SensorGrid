#include "io.h"

static RH_RF95 rf95(RFM95_CS, RFM95_INT);
static RHMesh* router;
static uint32_t MSG_ID = 0;
static uint8_t buf[sizeof(Message)] = {0};
static uint8_t controlBuffer[sizeof(Control)] = {0};
static struct Message *msg = (struct Message*)buf;
static struct Control *control = (struct Control*)controlBuffer;
static struct Message message = *msg;
static char* charBuf = (char*)buf;
uint8_t routingBuf[RH_MESH_MAX_MESSAGE_LEN];

static void clearBuffer()
{
    memset(buf, 0, sizeof(Message));
}

static void clearControlBuffer()
{
    memset(controlBuffer, 0, sizeof(Control));
}

void setupRadio()
{
    router = new RHMesh(rf95, config.node_id);
    
    rf95.setModemConfig(RH_RF95::Bw125Cr48Sf4096);
    if (!router->init())
        Serial.println(F("Router init failed"));
        
    if (!rf95.setFrequency(config.rf95_freq)) {
        fail(LORA_FREQ_FAIL);
    }
    Serial.print(F("FREQ: ")); Serial.println(config.rf95_freq);
    rf95.setTxPower(config.tx_power, false);
    //rf95.setCADTimeout(2000);
    router->setTimeout(1000);
    delay(100);
}

void reconnectClient(WiFiClient& client) {
  //close conncetion before a new request
  client.stop();
  char server[] = "54.152.62.254"; //SensorGrid API IP Address
  if (client.connect(server, 9022)) {
    Serial.println("connecting...");
  } else {
    Serial.println("Failed to reconnect");
  }
}

void postToAPI(WiFiClient& client,int fromNode, int id) {
  Serial.println("Starting to post to API...");
  char str[200];
  sprintf(str,
  "{\"ver\":%i,\"net\":%i,\"orig\":%i,\"id\":%i,\"bat\":%3.2f,\"ram\":%i,\"timestamp\":%i,\"data\":[%i,%i,%i,%i,%i,%i,%i,%i,%i,%i]}",
      msg->ver, msg->net, fromNode, id, (float)(msg->bat_100)/100, msg->ram, msg->timestamp,
      msg->data[0], msg->data[1], msg->data[2], msg->data[3], msg->data[4],
      msg->data[5], msg->data[6], msg->data[7], msg->data[8], msg->data[9]);
   client.println("POST /data HTTP/1.1");
   client.println("Content-Type: application/json");
   client.print("Content-Length: ");
   client.println(strlen(str));
   client.println();
   client.println(str);
   Serial.println("Post to API completed.");
   //for debugging
   while (client.available()) {
      char c = client.read();
      Serial.write(c);
     }
   if (!client.available()) {
    Serial.println("Client not available.");
    client.println("Connection: close"); //close connection before sending a new request
    client.println();
   }
}

static void sleep(int sleepTime)
{
    // TODO: may need minimum allowed sleep time due to radio startup cost
    if (sleepTime > 0) {
        rf95.sleep();
        GPS.standby();
        Serial.println(F("Sleeping for: ")); Serial.println(sleepTime);
        // Note: this delay will prevent display timeout, display update, and other protothread calls
        delay(sleepTime); // TODO: this might not be the best way to sleep. Look at SleepyDog: https://github.com/adafruit/Adafruit_SleepyDog
        GPS.sendCommand(""); // empty sendCommand in lieu of wakeup b/c wakeup freezes. See: https://github.com/adafruit/Adafruit_GPS/issues/25
    } else {
        Serial.print(F("Received bad sleep time: ")); Serial.println(sleepTime);
    }
}

bool sendCurrentMessage()
{ 
    fillCurrentMessageData();
    printMessageData(config.node_id);
    Serial.println(F("Sending current data"));
    clearControlBuffer();
    uint8_t len = sizeof(controlBuffer);
    uint8_t from;
    uint8_t errCode;
    uint8_t txAttempts = 5;
    bool success = false;
    if (oled_is_on)
        displayTx(config.collector_id);
    while (txAttempts > 0) {
        errCode = router->sendtoWait((uint8_t*)buf, sizeof(Message), config.collector_id);
        if (errCode == RH_ROUTER_ERROR_NONE) {
            // It has been reliably delivered to the next node.
            // Now wait for a reply from the ultimate server
            if (router->recvfromAckTimeout(controlBuffer, &len, 5000, &from)) {
                Serial.print(F("Received reply from: ")); Serial.print(from, DEC);
                Serial.print(F("; rssi: ")); Serial.println(rf95.lastRssi());
                if (oled_is_on)
                    displayRx(from, rf95.lastRssi());
                if (control->type == CONTROL_TYPE_SLEEP) {
                    // TODO: there should be a minimum allowed sleep time due to radio startup cost
                    sleep(control->data);
                }
                success = true;
            } else {
                Serial.println(F("recvfromAckTimeout: No reply, is collector running?"));
            }
        } else {
           Serial.println(F("sendtoWait failed. Are the intermediate nodes running?"));
        }
        router->printRoutingTable();
        if (success) {
          return true;
        } else {
          txAttempts--;
          if (txAttempts > 0) {
              Serial.print(F("Retrying data transmission x")); Serial.println(5-txAttempts);
          }
        }
    }
    Serial.println(F("Data transmission failed"));
    return false;
}

void printMessageData(int fromNode)
{
    Serial.print(F("FROM: ")); Serial.println(fromNode, DEC);
    Serial.print(F("VER: ")); Serial.println(msg->ver, DEC);
    Serial.print(F("BAT: ")); Serial.println((float)msg->bat_100/100);
    Serial.print(F("RAM: ")); Serial.println(msg->ram, DEC);
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

static char* logline(int fromNode, int id)
{
    char str[200]; // 155+16 is current theoretical max
    sprintf(str,
        "%i,%i,%i,%i,%3.2f,%i,%i,%i,%wii,%i,%i,%i,%i,%i,%i,%i,%i",
        msg->ver, msg->net, fromNode, id, (float)(msg->bat_100)/100, msg->ram, msg->timestamp,
        msg->data[0], msg->data[1], msg->data[2], msg->data[3], msg->data[4],
        msg->data[5], msg->data[6], msg->data[7], msg->data[8], msg->data[9]);
    return str;
}

static void writeLogLine(int fromNode, int id)
{
    char* line = logline(fromNode, id);
    Serial.print(F("LOGLINE (")); Serial.print(strlen(line)); Serial.println("):");
    Serial.println(line);
    if (config.log_file) {
        digitalWrite(SD_CHIP_SELECT_PIN, LOW);
        writeToSD(config.log_file, line);
        digitalWrite(SD_CHIP_SELECT_PIN, HIGH);
    }
}

static uint32_t getDataByTypeName(char* type)
{
    Serial.print(F("Getting data for type: ")); Serial.println(type);

    struct SensorConfig *sensor_config = sensor_config_head;
    do {
        if (!strcmp(type, sensor_config->id)) {
            int32_t val = sensor_config->read_function();
            return val;
        }
        sensor_config = sensor_config->next;
    } while (sensor_config != NULL);

    int32_t constant_value = atoi(type);
    if (constant_value) {
        return constant_value;
    }

    Serial.print(F("WARNING! Unknown named data type: ")); Serial.println(type);
    return 0;
}

static uint32_t getRegisterData(char* registerName, char* defaultType)
{
    char* type = getConfig(registerName);
    if (!type) {
        type = defaultType;
    }
    return getDataByTypeName(type);
}

static uint32_t getRegisterData(char* registerName)
{
    char* type = getConfig(registerName);
    if (type) {
        return getDataByTypeName(type);
    } else {
        //Serial.print(F("Data register ")); Serial.print(registerName);
        //Serial.println(F(" not configured"));
        return 0;
    }
}

void fillCurrentMessageData()
{
      clearBuffer();
      msg->ver = config.protocol_version;
      msg->bat_100 = (int16_t)(roundf(batteryLevel() * 100));
      msg->timestamp = rtc.now().unixtime();
      msg->ram = freeRam();
      memcpy(msg->data, {0}, sizeof(msg->data));
      msg->data[0] = getRegisterData("DATA_0");
      msg->data[1] = getRegisterData("DATA_1");
      msg->data[2] = getRegisterData("DATA_2");
      msg->data[3] = getRegisterData("DATA_3");
      msg->data[4] = getRegisterData("DATA_4");
      msg->data[5] = getRegisterData("DATA_5");
      msg->data[6] = getRegisterData("DATA_6");
      msg->data[7] = getRegisterData("DATA_7");
      msg->data[8] = getRegisterData("DATA_8");
      msg->data[9] = getRegisterData("DATA_9");
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

void receive()
{
  static uint8_t data[] = "ACK from server";
  uint8_t len = sizeof(buf);
  uint8_t from;
  if (router->recvfromAckTimeout(buf, &len, 5000, &from)) {
    if (config.node_id == 1) {
        Serial.print(F("REC request from :")); Serial.println(from, DEC);
        printMessageData(from);
    } 
    if (router->sendtoWait(data, sizeof(data), from) != RH_ROUTER_ERROR_NONE)
        Serial.println(F("sendtoWait failed"));
  } else {
    //Serial.println("Got nothing");
  }
}

void waitForInstructions()
{
  uint8_t len = sizeof(controlBuffer);
  uint8_t from;
  if (router->recvfromAckTimeout(controlBuffer, &len, 5000, &from)) {
      Serial.print(F("Recieved request from: ")); Serial.println(from, DEC);
      if (control->type == CONTROL_TYPE_SEND_DATA) {
          Serial.println(F("Received send-data request"));
          sendCurrentMessage();
      } else if (control->type == CONTROL_TYPE_SLEEP) {
          sleep(control->data);
      }
  } else {
    //Serial.println("Got nothing");
  }
}

void collectFromNode(int toID, uint32_t nextCollectTime, WiFiClient& client)
{
    Serial.print(F("Sending data request to node ")); Serial.println(toID);
    clearControlBuffer();
    clearBuffer();
    uint8_t msg_len = sizeof(Message);
    uint8_t len = sizeof(controlBuffer);
    uint8_t from;
    uint8_t dest;
    uint8_t id;
    uint8_t flags;
    uint8_t errCode;
    uint8_t txAttempts = 3;
    bool success = false;
    control->type = CONTROL_TYPE_SEND_DATA;
    while (txAttempts > 0) {
        // request the data
        errCode = router->sendtoWait((uint8_t*)control, len, toID);
        if (errCode == RH_ROUTER_ERROR_NONE) {
            // receive the data
            if (router->recvfromAckTimeout(buf, &msg_len, 5000, &from, &dest, &id, &flags)) {
                Serial.print("got reply from : "); Serial.print(from, DEC);
                Serial.print(" Msg ID: "); Serial.print(id, DEC);
                Serial.print(" rssi: "); Serial.println(rf95.lastRssi());
                // TODO: send data to the API
                printMessageData(from);
                //writeLogLine(from, id);
                success = true;
                // ack with OK SLEEP
                control->type = CONTROL_TYPE_SLEEP;
                control->data = nextCollectTime - millis();
                // don't hold up for send-sleep failures cd Sbut TODO: report these somehow
                if (router->sendtoWait(controlBuffer, len, from) != RH_ROUTER_ERROR_NONE)
                    Serial.println(F("ACK sendtoWait failed"));

                    while (!client.connected()) {
                      delay(1000);
                      reconnectClient(client);
                    }
                    postToAPI(client,from,id);

            } else {
                Serial.println(F("recvfromAckTimeout: Link to node broken."));
            }
        } else {
           Serial.println(F("sendtoWait failed. Are the intermediate nodes running?"));
        }
        if (success) {
          return;
        } else {
          txAttempts--;
          if (txAttempts > 0) {
              Serial.print(F("Retrying request for data transmission x")); Serial.println(3-txAttempts);
          }
        }
    }
    Serial.println(F("Request for data transmission failed"));
    router->printRoutingTable();
}

void _writeToSD(char* filename, char* str)
{
    static SdFat sd;
    Serial.print(F("Init SD card .."));
    if (!sd.begin(SD_CHIP_SELECT_PIN)) {
          Serial.println(F(" .. SD card init failed!"));
          return;
    }
    if (false) {  // true to check available SD filespace
        Serial.print(F("SD free: "));
        uint32_t volFree = sd.vol()->freeClusterCount();
        float fs = 0.000512*volFree*sd.vol()->blocksPerCluster();
        Serial.println(fs);
    }
    File file;
    Serial.print(F("Writing log line to ")); Serial.println(filename);
    file = sd.open(filename, O_WRITE|O_APPEND|O_CREAT);
    file.println(str);
    file.close();
    Serial.println(F("File closed"));
}

void writeToSD(char* filename, char* str)
{
    digitalWrite(8, HIGH);
    _writeToSD(filename, str);
    digitalWrite(8, LOW);
}

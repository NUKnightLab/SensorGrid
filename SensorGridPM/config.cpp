/**
 * Copyright 2018 Northwestern University
 */
#include "config.h"
#include <LoRaHarvest.h>

struct Config config;

uint32_t getTime() {
    return rtcz.getEpoch();
}

/**
 * We pull both pins 8 and 19 HIGH during SD card read. The default configuration
 * on the integrated LoRa M0 uses pin 8 for RFM95 chip select, but the WiFi
 * collector module with a LoRa wing uses pin 19 as the chip select. Since we have
 * not yet read the config file, it could be either one.
 *
 * No other RFM95 CS alternates are supported at this time
 */

bool Base::readConfig() {
  if (!readSDConfig(CONFIG_FILE)){
    file_read = true;
    return true;
  }
  return false;
}

bool WifiConfig::loadConfig() {
  if (file_read || Base::readConfig()) {
    wifi_ssid = getConfig("WIFI_SSID");
    wifi_password = getConfig("WIFI_PASSWORD");
    api_host = getConfig("API_HOST");
    api_port = (uint16_t)(atoi(
      getConfig("API_PORT", DEFAULT_API_PORT)));
    return true;
  } else {
      logln(F("ERROR: No config file found"));
      return false;
  }
}

bool RadioConfig::loadConfig() {
  if (file_read || Base::readConfig()) {
    SD_CHIP_SELECT_PIN = (uint32_t)(atoi(
      getConfig("SD_CHIP_SELECT_PIN", DEFAULT_SD_CHIP_SELECT_PIN)));
    RFM95_CS = (uint32_t)(atoi(
      getConfig("RFM95_CS", DEFAULT_RFM95_CS)));
    RFM95_RST = (uint32_t)(atoi(
      getConfig("RFM95_RST", DEFAULT_RFM95_RST)));
    RFM95_INT = (uint32_t)(atoi(
      getConfig("RFM95_INT", DEFAULT_RFM95_INT))); 
    return true;
  } else {
      logln(F("ERROR: No config file found"));
      return false;
  }
}

bool Config::loadConfig() {
  int default_rfm_cs = atoi(DEFAULT_RFM95_CS);
  int alt_rfm_cs = atoi(ALTERNATE_RFM95_CS);
  digitalWrite(default_rfm_cs, HIGH);
  digitalWrite(alt_rfm_cs, HIGH);
  if (file_read || Base::readConfig()) {
      digitalWrite(default_rfm_cs, LOW);
      digitalWrite(alt_rfm_cs, LOW);
      network_id = (uint32_t)(atoi(getConfig("NETWORK_ID")));
      node_id = (uint8_t)(atoi(getConfig("NODE_ID", "0")));
      collector_id = (uint8_t)(atoi(getConfig("COLLECTOR_ID")));
      rf95_freq = static_cast<float>(atof(getConfig("RF95_FREQ")));
      tx_power = (uint8_t)(atoi(getConfig("TX_POWER")));
      sensorgrid_version = (uint8_t)atoi(
          getConfig("SENSORGRID_VERSION", DEFAULT_SENSORGRID_VERSION));
      log_file = getConfig("LOGFILE", DEFAULT_LOG_FILE);
      display_timeout = (uint32_t)(atoi(
          getConfig("DISPLAY_TIMEOUT", DEFAULT_DISPLAY_TIMEOUT)));
      node_type = (uint8_t)(atoi(getConfig("NODE_TYPE")));
      heartbeat_period = (uint32_t)(atoi(
          getConfig("HEARTBEAT_PERIOD", DEFAULT_HEARTBEAT_PERIOD)));
      sample_period = (uint32_t)(atoi(
          getConfig("SAMPLE_PERIOD", DEFAULT_SAMPLE_PERIOD)));
      collection_period = (uint32_t)(atoi(
          getConfig("COLLECTION_PERIOD", DEFAULT_COLLECTION_PERIOD)));
      Serial.print("Loading LoRa config ..");
      RadioConfig::loadConfig();
      Serial.println(".. done");
  }
  if (node_id == 0) {
      Serial.println("Node ID not configured. Using Lower byte of Si7021 serial number A.");
      Adafruit_Si7021 _sensor = Adafruit_Si7021();
      _sensor.begin();
      node_id = (byte)_sensor.sernum_a;
      Serial.print("NODE ID: ");
      Serial.println(node_id);
  }
  if (node_id == 0) {
    Serial.println("Unable to set Node ID. Check configuration or Si7021 connectivity.");
    while(1);
  }
  return true;
}
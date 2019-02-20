/*
#include "tests.h"
#include "config.h"

using namespace aunit;

struct Config test_config;


test(simple1) {
    assertTrue(true);
}

test(WifiDefault) {
    test_config.loadConfig();
    assertEqual(test_config.api_port, DEFAULT_API_PORT);
}

test(RadioDefault) {
    test_config.loadConfig();
    assertEqual(test_config.SD_CHIP_SELECT_PIN, DEFAULT_SD_CHIP_SELECT_PIN);
    assertEqual(test_config.RFM95_CS, DEFAULT_RFM95_CS);
    assertEqual(test_config.RFM95_RST, DEFAULT_RFM95_RST);
    assertEqual(test_config.RFM95_INT, DEFAULT_RFM95_INT);
}

test(ConfigDefault) {
    test_config.loadConfig();
    assertEqual(test_config.sensorgrid_version, DEFAULT_SENSORGRID_VERSION);
    assertEqual(test_config.log_file, DEFAULT_LOG_FILE);
    assertEqual(test_config.display_timeout, DEFAULT_DISPLAY_TIMEOUT);
    assertEqual(test_config.heartbeat_period, DEFAULT_HEARTBEAT_PERIOD);
    assertEqual(test_config.sample_period, DEFAULT_SAMPLE_PERIOD);
    assertEqual(test_config.collection_period, DEFAULT_COLLECTION_PERIOD);
}

*/
# SensorGrid data collector nodes

A SensorGrid data collector is responsible for collecting data from the nodes in the network and posting that data to the API.

Two collector types are supported:

 1. Serial Collector
 2. WiFi Collector

## Serial Collector

A Serial Collector is simply a SensorGrid node configured as a collector and connected to a computer serial port. The node itself handles data acquisition from sensor nodes. Code (TBD) running on the computer parses log lines from the node's serial output and posts the data to the API. This approach could work well, for example, with a Raspberry Pi setup to handle the serial data.

The standard modular configuration of a serial collector is simply to stack a LoRa M0 Feather with an Adalogger Featherwing and configure the node as a collector (NODE_TYPE 4)


## WiFi Collector

A Wifi Collector handles both the collection of data and the posting of data to the API, via WiFi of course.

It is important to note: in order to handle pin conflict issues, the modular configuration of a WiFi collector is different from other nodes. The WiFi collector requires the following physical stack (note particularly the use of a LoRa wing rather than LoRa core module):

 * Adalogger Featherwing https://www.adafruit.com/product/2922
 * LoRa Featherwing https://www.adafruit.com/product/3231
 * WiFi M0 Feather (WINC1500) https://www.adafruit.com/product/3010

The LoRa CS, RST, and INT pins will need to be wired. The typical connections are:

 * CS 19
 * RST 11
 * INT 6

The collector configuration will require settings for the alternative pinouts, for the API endpoint, and for WiFi connectivity. An example configuration file follows:

```
PROTOCOL_VERSION 1
NODE_ID 1
RF95_FREQ 915.0
RFM95_CS 19 (flyout pin F)
RFM95_RST 11 (flyout pin A)
RFM95_INT 6 (flyout pin D)
TX_POWER 10
LOGFILE sensorgrid.log
DISPLAY 0
NODE_TYPE 4
ORDERED_NODE_IDS 2,3,4
COLLECTION_PERIOD 60
WIFI_SSID YourWifiSSID
WIFI_PASSWORD YourWifiPassword
API_HOST API.HOST.IP.ADDRESS
API_PORT 80
```

Note, you will need to set values for WIFI_SSID, WIFI_PASSWORD, and API_HOST. API_PORT is configurable and defaults to 80. Note that DNS is not currently supported, so an IP address is required for API_HOST


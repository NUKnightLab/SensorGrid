# SensorGrid Deployment

## Power

Nodes can be powered by a USB source (e.g. by plugging into a computer USB port), or more commonly by a 3.7v lithium ion polymer battery (e.g. https://www.adafruit.com/product/1578, https://www.adafruit.com/product/258, https://www.adafruit.com/product/328)

Important: Strain relief is strongly recommended with these batteries. The solder connections to the batteries are very fragile and will break if the wires are not constrained from movement.

If USB power is provided with a battery attached, the battery will recharge. However, this recharge will be slow since the node is also being powered. Thus, it is recommended to use a dedicated charger such as Adafruit's USB Lilon/LiPoly charger (https://www.adafruit.com/product/259) or the versitile USB/DC/Solar Lithium Ion/Polymer charger - v2 (https://www.adafruit.com/product/390)

Please use only a single battery attached to a node via the provided battery connector. Improper use of LiPoly batteries can be unsafe. For more information, see Adafruit's excellent introduction to LiPoly batteries:
https://learn.adafruit.com/li-ion-and-lipoly-batteries?view=all

## Configuration

### Currently supported configuration options

The following configuration options are available

**PROTOCOL_VERSION** 1

1 is currently the only supported version number

**NODE_ID** (1 - 255)

**RF95_FREQ**

Defaults to 915.0. This is currently the only supported radio frequency. For operation outside the US, please check your local ordinances to see if a different frequency is required. This may require a different radio module than the specified module.

**TX_POWER** (5 - 23)

Radio tranmission power. Higher power for greater distances but with battery life tradeoff. Use the lowest power that is practical for your application.

**DISPLAY** (1 or 0)

Set to 1 if you have an OLED FeatherWing attached. Otherwise 0

**LOGFILE** sensorgrid.log

The logfile used to log events according to the LOGMODE setting

**DISPLAY_TIMEOUT** 60

OLED display timeout in seconds. Defaults to 60

**NODE_TYPE** (1 - 5)

Standard routing (requires dedicated router nodes):

  1 COLLECTOR: collector used for standard routing
  2 ROUTER
  3 SENSOR

Ordered collection routing:

  4 ORDERED_COLLECTOR
  5 ORDERED_SENSOR_ROUTER

**COLLECTOR_ID**

The ID of the collector this node sends data to (standard routing only)

**ORDERED_NODE_IDS**

Required only on an ordered collector node. Ordered comma-delimited list of nodes to collect from. (Ordered collection routing only)

**DATA_0** .. **DATA_9**

Data registers to be configured with named data types below

The following data register data types are supported:

 - GPS_FIX
 - GPS_SATS
 - GPS_SATFIX
 - GPS_LAT_DEG
 - GPS_LON_DEG
 - GROVE_AIR_QUALITY_1_3
 - SHARP_GP2Y1010AU0F_DUST
 - SI1145_IR
 - SI1145_UV
 - SI1145_VIS
 - SI7021_HUMIDITY
 - SI7021_TEMP


### Currently unsupported configuration options

Legacy, deprecated, or temporarily removed pending implementation

**NETWORK_ID**

**WIFI_SSID**

Your SSID (Not currently used)

**WIFI_PASS**

YourPassword (Not currently used)

**API_SERVER**

your.api.ip.address (Not currently used)

**API_PORT**

YourApiPort (Not currently used)

**API_HOST**

YourApiHost (Not currently used)

**LOGMODE**

Determines which events are logged to the SD card. Not currently used



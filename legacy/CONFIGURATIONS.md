# SensorGrid Configurations

## Modular configurations

Choose your configuration! You should do this before you start soldering things since the configuration will impact header choices. There are a lot of options. If in doubt, solder stacking headers onto everything and figure out your configuration as you go along -- however, judicious header choices can make for a more compact and nicer looking product!

Every node requires a LoRa Feather and an Adalogger FeatherWing. All other components of a node depend on your requirements. Use the configuration tables below to match configurations to your requirements.

### Sensor options

We have built, or are currently actively developing, support for the following sensor options:

  * GPS
  * Temperature/humidity
  * Light (Visible/UV/IR)
  * air-quality/dust (coming soon!)

TODO: one or more temp/hum/light boards seem possibly fried. Need to check this out be sure we don't have compatibility issues with this board

### Single-stack configurations

  * No extra proto boards required
  * Good for collector and repeater nodes
  * Either/or OLED/Sensor top-component
  * OLED+Sensor can be used if sensor does not need to be on top

|       | 1a     | 2a     | 3a     | 4a     |
|-------|--------|--------|--------|--------|
| Stack |        |        |        | OLED   |
|       |        | OLED   | Sensor | Sensor |
|       | Logger | Logger | Logger | Logger |
|       | LoRa   | LoRa   | LoRa   | LoRa   |

1a. Single-stacked (Communicative)\* Senseless Logger

Requirements:
  * LoRa
  * Logger

Uses:
  * A simple way to get started just testing out the protocol
  * Can be used as a collector and/or repeater node
  * Coupled with one or more #3 nodes provides the simplest SensorGrid configuration for logging remote sensor data

\* Note: all nodes, as currently specified, are communicative nodes. There is a currently unsupported use-case for non-communicative nodes that simply log their own data. Support for non-communicative logger nodes is pending for future development tracks, depending on perceived demand


2a. Single-stacked Senseless Logger with Readout

Requirements:
  * LoRa
  * Logger
  * OLED

Uses:
  * Adds a readout display to the simplest setup of #1
  * Currently only displays time (more info TBD)


3a. Single-stacked Sensing Logger

Requirements:
  * LoRa
  * Logger
  * Sensor

Uses:
  * The simplest node that is actually a sensor node
  * Current support for temperature, humidity, light (Visible, UV, IR)
  * Dust/air quality support coming soon!


4a. Single-stacked Sensing Logger with Readout

NOTE: This configuration is not recommended for many sensor types. Only use if your sensor does not need to be top-stack

Requirements:
  * LoRa
  * Logger
  * Sensor
  * OLED

Uses:
  * A compact sensing communicative logger for use with sensors that do not need to be at the top of the stack
  * Can be used with temperature or humidity sensors
  * Single stacked OLED configuration will probably be compatible with the dust sensor (since the dust sensor itself will not be part of the stacked configuration) but TBD to be certain
  * Not recommended for use with light sensors



### Double-stack configurations (requires Adafruit doubler proto https://www.adafruit.com/products/2890)

With minimal additional soldering, the Feather doubler proto board provides much more flexibility than single-stacked configurations. For configurations that require multiple top-stack components, a dble or triple configuration is essential.

There are several doubler configurations that are operationally identical to single-stack configurations. The doubler provides a wider, flatter configuration which may be desirable.


|       | 1b             | 2b             | 3b             | 4b             |
|-------|----------------|----------------|----------------|----------------|
| Stack |                | OLED           | Sensor         | OLED -- Sensor |
|       | LoRa -- Logger | LoRa -- Logger | LoRa -- Logger | LoRa -- Logger |

1b - 3b requirements and usages are identally to their single-stacked counterparts (see 1a-3a above)

4b.
Requirements: (see #4b above)

Uses:
  * See #4a above
  * Also compatible with sensors required to be top-stack, e.g.: light sensors


|       | 5b              | 6b              |
|-------|-----------------|-----------------|
|       |                 | OLED            |
| Stack | Sensor - Sensor | Sensor - Sensor |
|       | LoRa  -- Logger | LoRa  -- Logger |


5b.
Requirements:
  * LoRa
  * Logger
  * Sensors

Uses:
  * Multiple top-stack sensors (e.g. a light sensor and GPS)
  * This is the simplest configuration for using GPS with an additional top-stack sensor

6b.
Requirements:
  * LoRa
  * Logger
  * Sensors
  * OLED

Uses:
  * Multiple sensors where a readout is desired and at least one sensor does not need to be top-stack (e.g. GPS + temp/humidity)

### 1-node
### 2-node time-only SensorGrid

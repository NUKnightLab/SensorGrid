# SensorGrid Building Guide

Be sure to take a look at the Planning Guide first. Most relevant information is there. For actual Feather and FeatherWing assembly, please see the Adafruit tutorial:
https://learn.adafruit.com/adafruit-feather-m0-basic-proto/assembly


## Additional notes/tips:

  * Keep your stack planning in mind. Desoldering is much less fun than soldering. If you're not sure what kind of header to use, go with a stacking header for maximum flexibility.

  * When soldering stacking headers, the breadboard trick shown in the Adafruit tutorial does not work so well. Some things that help to keep the headers straight:
    - tack-solder one or two pins and double-check the alignment before doing all the pins
    - it can help to use an already soldered module as a plug-in base to get the spacing and alignment right
    - once a pin or two are soldered, it can be helpful to use the vice. Probably not before that though
    - mostly, I just try to stand my headers up straight and hope for the best. They don't always turn out the greatest though

  * If you are still planning things out, consider wire wrapping instead of soldering where you can get away with it. Not so much for the Feather headers, but, e.g. if you are planning out a new sensor module

  * Some additional prototyping tools that might come in handy:
    - breadboard and jumper wires
    - breadboard terminal blocks
    - specialized Feather terminal (https://www.adafruit.com/products/3173)
    - specialized Feather terminal board (https://www.adafruit.com/products/2926)

# Tutorial: Build a SensorGrid Network with Ordered Node Collection Configuration

Ordered node collection is one of two supported routing/data collection schemes supported by SensorGrid. While it requires the additional configuration of an ordered list of sensor nodes on the collector, dedicated routers are not required, making it a more straightforward approach to deployment. Furthermore, ordered nodes can shut off their radios between requests, significantly extending battery life.

This tutorial will detail the steps for creating a SensorGrid Network with an ordered node configuration. Nodes can either be gas/dust sensors, or light (visible/uv/ir)/humidity/temperature sensors -- both types are covered here. To simplify, sensor components may be eliminated from this tutorial at will. E.g. one could build merely a gas sensor, or simply a light sensor.

Due to space limitations on the protoboard, a larger configuration would be needed to accomodate all of the sensors covered. This could be done by using a tripler base in lieu of a doubler, making the required room for all components.

## Build the collector node

### Components
Adafruit Feather short stacking headers
Adafruit M0 LoRa Feather
Adafruit Adalogger Feather Wing

## Build the sensor nodes

Note: this doubler configuration will accomodate a single sensor module. To include both modules on a node, use a tripler instead of a doubler.

### Components
Adafruit proto doubler (or tripler)
Adafruit Feather short stacking headers
Adafruit M0 LoRa Feather
Adafruit Adalogger Feather Wing
Adafruit OLED Feather Wing


## Build the air quality / dust module

### Components

Adafruit proto
Adafruit Feather male headers
Sharp dust sensor
Seeed studio Grove Air Quality sensor

## Build the light / temp / humidity module

### Components

Adafruit proto
Adafruit Feather male headers
Adafruit SI1145 light/UV/IR sensor
Adafruit SI7021 temperature/humidity sensor

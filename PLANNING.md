# SensorGrid Planning Guide

A little bit of planning will go a long way toward obtaining the parts you really need and building the correct configuration. Better planning docs will become available as we gather better information about SensorGrid's abilities and limitations.


## Plan your network

A SensorGrid network consists of an ad-hoc deployment of one (actually usually at least two) or more SensorGrid nodes. Each node plays a set of roles which may include:

  * sensor node
  * collector
  * repeater

All nodes are, in effect, repeaters. So a node is *only* a repeater if it is not a sensor or collector. A node may be one, two, or all three of these roles.

### Sensor nodes

Sensor nodes collect data from the environment via an integrated sensor module. That data is logged to an SD card and is simultaneously broadcast to the network.

To be a true "sensor grid" your network will need at least one sensor node. More likely you will have several to be distributed over a local area.

### Collector

A collector may or may not also be a sensor. A collector is such in that, in addition to logging its own data to SD, it is also configured to log data it receives from other nodes.

  * The simplest collector logs all incomming data to the SD card
  * A collector may be configured with a WiFi module to write data to a remote API
  * Other collector types TBD

You will probably want at least one collector in your network

### Repeater

All nodes are repeaters. In addition to broadcasting their own data (in the case of sensor nodes) all SensorGrid nodes re-transmit received data, thus re-broadcasting it to the network.

In some cases you may want additional repeater nodes that are neither sensors nor collectors. Repeater nodes serve the simple purpose of expanding the range of your SensorGrid network.

### Plan it!

(TBD more info about expected ranges, node capacity, etc.)

To plan your network, fill in the following:

I will need _ _ _ _ _ sensor nodes (which may also be collectors/repeaters)

In addition to sensor nodes, I will have _ _ _ _ _ collector(s) that are not sensors

and _ _ _ _ _ _ repeater(s) that are also not sensors


## Plan your nodes

For each node you will need to:

  * Plan your layers
  * Determine your parts
  * Build the modules
  * Assemble the node

### Plan your layers

Planning your layers includes:

  1. Determine top-layer
  2. Determine the base
  3. Choosing headers

#### 1. Determine top-layer surface level components

  - [ ] OLED digital readout (currently optional and displays only the time)
  - [ ] GPS
  - [ ] Top level sensor. E.g. a light sensor that cannot be obstructed


#### 2. Determine your base

**How many boxes did you check in step 1?**

  1. No extra protoboards are needed (single-stack configuration)
  2. You will need an Adafruit Feather Doubler proto board (https://www.adafruit.com/products/2890)
  3. You will need an Adafruit Feather Tripler proto board (https://www.adafruit.com/products/3417)

**Do you need WiFi?**

  **No:** The proto board selected above will be your base (or simply a single-stack configuration if no-extra proto). Thus your configuration is one of:
  1. Single-stack
  2. Double-stack (using a doubler proto to allow 2 top-layer components)
  3. Triple-stack (using a tripler proto to allow 3 top-layer components)

  **Yes:** You may need to convert your configuration
  1. Single-stack: Build the WiFi module on a doubler and convert to double-stack configuration
  2. Double-stack: Build the WiFi module on a doubler. This is your base
  3. Triple-stack: Build the WiFi module on a tripler. This is your base
    - No extra proto board (single-stack) use a double-stack WiFi and convert to

#### 3. Choose your headers

  1. Each top-layer component can use standard male header (https://www.adafruit.com/products/3002)
  2. Middle-layer components require stacking headers (https://www.adafruit.com/products/2830)
  3. Base layer can use regular Feather female headers (https://www.adafruit.com/products/2886) or short Feather headers (https://www.adafruit.com/products/2940) Note: for a WiFi module, you will probably want to use stacking headers. See WiFi tutorial for details

**If in doubt, use stacking headers for everything -- this is the most versitile option for an uncertain future**


### Determine your parts

  - [x] Feather M0 RFM95 LoRa (https://www.adafruit.com/products/3178)
  - [x] Adalogger FeatherWing (https://www.adafruit.com/products/2922)
  - [x] Headers (see above)

  - [ ] OLED FeatherWing (https://www.adafruit.com/products/2900)
  - [ ] GPS FeatherWing (https://www.adafruit.com/products/3133)


  - [ ] WiFi (see separate WiFi tutorial, TBD)
  - [ ] Sensors (see separate sensor tutorials, TBD)

### Build the node modules

  * For the WiFi module, see the WiFi tutorial (TBD)
  * For sensor modules, see sensor tutorials (TBD)
  * For remaining modules, solder headers as planned above. The Adafruit tutorial for Feather assembly provides a good overview of assembly as appropriate for all Feathers and FeatherWings (https://learn.adafruit.com/adafruit-feather-m0-basic-proto/assembly). Be sure to solder the right headers onto the right component boards. If in doubt, use stacking headers

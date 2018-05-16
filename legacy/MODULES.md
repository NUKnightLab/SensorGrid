# Modules

## Feather M0 LoRa (https://www.adafruit.com/products/3178)

**Required:** Yes

The core module required on all SensorGrid nodes

## Adalogger Featherwing (https://www.adafruit.com/products/2922)

**Required:** Generally, except for advanced usage

Used for configuration, logging, and timekeeping. This module is generally
required on all nodes.

### Making the logger optional

It may be possible to elminate the Adalogger from your nodes with some extra work. Note this is advanced usage and is not officially supported. This will necessarily require compiling unique versions of the code to each node.

The configuration file on the SD card is the key component to making SensorGrid as plug-and-play as possible. For most users, no coding is required to build a SensorGrid. In order to remove the Adalogger as a node requirement, you will need to hardcode configuration values. The easiest way to do this is probably to find the `DFAULT_` values and change them. Note, just depending on the defaults alone will not make for a very useful SensorGrid network. At the very least, you will need to set the ID of each node.

## OLED Featherwing (https://www.adafruit.com/products/2900)

**Required:** No, but strongly recommended (particularly for logging nodes)

The OLED display shows some useful information including battery level, date/time, and last-message info. In order to save battery life, the display times out and is normally off. Hold the C button to turn it back on. Hold the C button longer to enter shutdown mode before turning off the node.

The DISPLAY configuration defaults to 0. Be sure to set it to 1 in the config file for the OLED display to work. Note: a node will not work correctly if DISPLAY is set to 1 but no OLED is present.

Important: There is no available shutdown function if no OLED is intalled and configured. There is a chance of logfile corruption when turning off a node without using the C-button shutdown feature.

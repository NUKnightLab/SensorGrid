# SensorGrid

## What is it?

SensorGrid is Knight Lab's experimental DIY prototyping system for ad hoc sensor network deployment. Designed around Adafruit's Atmel-based Feather prototyping ecosystem, SensorGrid is modular, extensible, and open source all the way down to its core components

## Should I use it?

If you can answer yes to these questions:

  * Are you okay with using a system that is experimental, largely untested, and will be rapidly changing as we continue to develop it?
  * Can you tolerate potential foibles such as lost data packets, time precision that is probably not to the second, etc?
  * Is your data slow changing and does it have relatively low sampling-period requirements (minutes & hours rather than milliseconds or seconds)
  * Is your data concentrated over a small geographic area (e.g. a campus or neighborhood. Not distributed over a city or county)?
  * Are you ready to roll up your sleeves and bust out the soldering iron?

# Documentation

See these additional docs:

  * Planning guide
  * Configurations guide. An attempt to tabulate configurations. Might go away


## Some notes
TODO: Move these to a module notes doc

### LoRa (Feather)
Note: 32u4 boards are no longer generally supported by SensorGrid. Be sure to get the M0/SAMD LoRa board which is Adafruit #3178

Product: https://www.adafruit.com/products/3178
Tutorial: https://learn.adafruit.com/adafruit-feather-m0-radio-with-lora-radio-module?view=all


### OLED (FeatherWing)
Product: https://www.adafruit.com/products/2900
Tutorial: https://learn.adafruit.com/adafruit-oled-featherwing?view=all

I2C address 0x3C (Fixed) Uses SDA & SCL pins for display communication
~10mA current draw

Buttons:
 * Button A #9
 * Button B #6 (has 100K pullup)
 * Button C #5


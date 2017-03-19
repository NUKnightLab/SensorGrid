# SensorGrid

## What is it?

SensorGrid is Knight Lab's experimental DIY prototyping system for ad hoc sensor network deployment. Designed around Adafruit's Atmel-based Feather prototyping ecosystem, SensorGrid is modular, extensible, and open source all the way down to its core components

SensorGrid's primary goals include:
  * ease of deployment
  * modularity
  * adaptability
  * openness

SensorGrid is designed to quickly and easily deploy a wireless network of environmental data collection. The primary use case is environmental data collection around a campus area for use by students and researchers for environmental data reporting classes and projects. We hope to expand these use cases as we build out SensorGrid's capabilities and test its limititations.


## Should I use it?

If you can answer yes to these questions:

  * Are you okay with using a system that is experimental, largely untested, and will be rapidly changing as we continue to develop it?
  * Can you tolerate potential foibles such as lost data packets, time precision that is probably not to the second, etc?
  * Is your data slow changing and does it have relatively low sampling-period requirements (minutes & hours rather than milliseconds or seconds)
  * Is your data concentrated over a small geographic area (e.g. a campus or neighborhood. Not distributed over a city or county)?
  * Are you ready to roll up your sleeves and bust out the soldering iron?

# Documentation

See these additional docs:

  * [Planning guide](https://github.com/NUKnightLab/SensorGrid/blob/master/PLANNING.md)
  * [Configurations guide](https://github.com/NUKnightLab/SensorGrid/blob/master/CONFIGURATIONS.md) An attempt to tabulate configurations. Might go away
  * [Building guide](https://github.com/NUKnightLab/SensorGrid/blob/master/BUILD.md)
  * (TBD) WiFi building tutorial
  * (TBD) Sensor integration tutorial


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


## Pinouts
  * 1 (also 6 & 12) Used by WiFi WINC1500 Module
  * 3 RFM95 Interrup (M0)
  * 4 RFM95 Reset
  * 5 OLED Button C
  * 6 OLED Button B
  * 6 (also 1 & 12) Used by WiFi WINC1500 Module
  * 7 RFM95 Interrupt (32u4)
  * 8 RFM95 Chip select (Must be HIGH during Adalogger read/write)
  * 9 OLED Button A
  * 10 SD Chip select
  * 12 (proposed) Dust Sensor LED power
  * 12 (also 1 & 6) Used by WiFi WINC1500 Module
  * 13 Red LED on main Feather board (LoRa)
  * A0 (proposed) Dust Sensor Read
  * A7 Battery read on M0
  
### Adalogger pins
  * 3.3V & GND: used for both SD and RTC
  
  #### RTC
  * SCL: RTC -- I2C clock pin, connect to your microcontrollers I2C clock line. This pin has a 10K pullup resistor to 3.3V
  * SDA: RTC -- I2C data pin, connect to your microcontrollers I2C data line. This pin has a 10K pullup resistor to 3.3V
  
  NOTE: You MUST have a coin cell installed for the RTC to work, if there is no coin cell, it will act strangely and possibly hang the Arduino when you try to use it, so ALWAYS make SURE there's a battery installed, even if it's a dead battery.
  
  #### SD Card
  * SPI Clock (SCK) - output from feather to wing
  * SPI Master Out Slave In (MOSI) - output from feather to wing
  * SPI Master In Slave Out (MISO) - input from wing to feather
  * The SDCS pin is the chip select line. GPIO 10 on Atmel M0 or 32u4 (can be changed by cutting the trace and hard-wiring)

When the SD card is not inserted, these pins are completely free. MISO is tri-stated whenever the SD CS pin is pulled high

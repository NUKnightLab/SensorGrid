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

### LoRa Feather pins

From https://learn.adafruit.com/adafruit-feather-m0-radio-with-lora-radio-module?view=all

  * GND - this is the common ground for all power and logic
  * BAT - this is the positive voltage to/from the JST jack for the optional Lipoly battery
  * USB - this is the positive voltage to/from the micro USB jack if connected
  * EN - this is the 3.3V regulator's enable pin. It's pulled up, so connect to ground to disable the 3.3V regulator
  * 3V - this is the output from the 3.3V regulator, it can supply 500mA peak
  * #0 / RX - GPIO #0, also receive (input) pin for Serial1 (hardware UART), also can be analog input
  * #1 / TX - GPIO #1, also transmit (output) pin for Serial1, also can be analog input 
  * #20 / SDA - GPIO #20, also the I2C (Wire) data pin. There's no pull up on this pin by default so when using with I2C, you may need a 2.2K-10K pullup.
  * #21 / SCL - GPIO #21, also the I2C (Wire) clock pin. There's no pull up on this pin by default so when using with I2C, you may need a 2.2K-10K pullup.
  * #5 - GPIO #5
  * #6 - GPIO #6
  * #9 - GPIO #9, also analog input A7. This analog input is connected to a voltage divider for the lipoly battery so be aware that this pin naturally 'sits' at around 2VDC due to the resistor divider
  * #10 - GPIO #10
  * #11 - GPIO #11
  * #12 - GPIO #12
  * #13 - GPIO #13 and is connected to the red LED next to the USB jack
  * A0 - This pin is analog input A0 but is also an analog output due to having a DAC (digital-to-analog converter). You can set the raw voltage to anything from 0 to 3.3V, unlike PWM outputs this is a true analog output
  * A1 thru A5 - These are each analog input as well as digital I/O pins.
  * SCK/MOSI/MISO (GPIO 24/23/22)- These are the hardware SPI pins, you can use them as everyday GPIO pins (but recommend keeping them free as they are best used for hardware SPI connections for high speed.

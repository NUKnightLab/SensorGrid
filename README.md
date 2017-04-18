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

# Dependencies

  * RadioHead http://www.airspayce.com/mikem/arduino/RadioHead/
  * SDFat https://github.com/greiman/SdFat
  * WiFi101 https://github.com/arduino-libraries/WiFi101
  * KnightLab_SDConfig https://github.com/NUKnightLab/KnightLab_SDConfig
  * AdaFruit_GPS https://github.com/adafruit/Adafruit_GPS
  * KnightLab_GPS https://github.com/NUKnightLab/KnightLab_GPS
  * AdaFruit-GFX-Library https://github.com/adafruit/Adafruit-GFX-Library
  * AdaFruit_SSD1306 https://github.com/adafruit/Adafruit_SSD1306
  * AdaFruit Feather_OLED https://github.com/adafruit/Adafruit_FeatherOLED
  * AdaFruit RTCLib https://github.com/adafruit/RTClib
  
TODO: is it possible to make these optional?:
  * WiFi101
  * AdaFruit_GPS
  * KnightLab_GPS
  * AdaFruit_SSD1306
  

# Documentation

See these additional docs:

  * [Planning guide](https://github.com/NUKnightLab/SensorGrid/blob/master/PLANNING.md)
  * [Configurations guide](https://github.com/NUKnightLab/SensorGrid/blob/master/CONFIGURATIONS.md) An attempt to tabulate configurations. Might go away
  * [Building guide](https://github.com/NUKnightLab/SensorGrid/blob/master/BUILD.md)
  * [Module guide](https://github.com/NUKnightLab/SensorGrid/blob/master/MODULES.md) overview of the parts that makeup nodes
  * (TBD) WiFi building tutorial
  * (TBD) Sensor integration tutorial

## Warning about GPS!!

Besides module incompatibilities with the GPS module (see below), for the time being, the GPS is considered generally unusable. We are seeing corrupted NMEA strings like the following:

$GPGGA,163232.000,4203.0383,N,08740.4182$0,A$GPGGA,163233.000,4203.0383,N,08740.,33,.$GPGGA,163234.000,4203.0383,N,087
$GPGGA,164328.000,4203.0390,N,08740.40,0.1$GPGGA,164329.000,4203.0390,N,08740.4132$0,*$GPGGA,164330.000,4203.0390,N,08
$GPGGA,164846.000,4203.0380,N,08740.41W.48,$GPGGA,164847.000,4203.0380,N,08740.41W488,$GPGGA,164848.000,4203.0380,N,08

One immediate side effect is the parsing out of these longitude values into the wrong E/W hemisphere. There are likely other major issues given the weirdness of these NMEA. It appears as though one string is being over-written by another at an offset. It is not clear if this is something in the current code base or in the KnightLab_GPS library. In general we do not recommend using GPS for the time being.

## Module Incompatibilities!!

Not everything plays together nicely in the Feather ecosystem. Here are some specific known issues to keep in mind when planning your SensorGrid nodes

  * **Adalogger SD card writing and Ultimate GPS incompatible**

While, theoretically, these should work together, we are seeing lockup after writing log lines to the SD card when a GPS is attached. This problem should be solvable, but in the mean time, **turn off logging** nodes with GPS

  * **Adalogger and WINC1500 WiFi unable to share SPI bus**

We have yet to figure out how to get these two to share the bus nicely. It seems that the WINC1500 does not like losing access to the bus, and even trying full WiFi setup after the fact does not manage the issue. For the time being, if the current node has WiFi, the log file will not be written. No additional configuration or changes are required.

  * **Possible conflict between WiFi and GPS reliability**

Need to test this out some more, but some early indications that we are not get good (any?) GPS data when there is a WiFi module attached. It may be necessary to avoid having WiFi nodes with GPS. This should be tested only after resolving the issue of corrupted NMEA strings noted above.

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

Be sure pin #8 is HIGH when reading/writing SD card


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

  #### Radio control pins:
  * #8 - used as the radio CS (chip select) pin (pull HIGH when not using radio!! Must be HIGH during Adalogger read/writes)
  * #3 - used as the radio GPIO0 / IRQ (interrupt request) pin.
  * #4 - used as the radio Reset pin

There are also breakouts for 3 of the RFM's GPIO pins (IO1, IO2, IO3 and IO5). You probably wont need these for most uses of the Feather but they are available in case you need 'em!

The CS pin (#8) does not have a pullup built in so be sure to set this pin HIGH when not using the radio!


### Feather WINC1500 (Currently not supported)

  * GND - this is the common ground for all power and logic
  * BAT - this is the positive voltage to/from the JST jack for the optional Lipoly battery
  * USB - this is the positive voltage to/from the micro USB jack if connected
  * EN - this is the 3.3V regulator's enable pin. It's pulled up, so connect to ground to disable the 3.3V regulator
  * 3V - this is the output from the 3.3V regulator, it can supply 600mA peak
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
  * SCK/MOSI/MISO (GPIO 24/23/22)- These are the hardware SPI pins, you can use them as everyday GPIO pins (but recommend keeping them free as they are best used for hardware SPI connections for high speed)
  * #2 - used as the ENable pin for the WiFi module, by default pulled down low, set HIGH to enable WiFi
  * #4 - used as the Reset pin for the WiFi module, controlled by the library
  * #7 - used as the IRQ interrupt request pin for the WiFi module, controlled by the library
  * #8 - used as the Chip Select pin for the WiFi module, used to select it for SPI data transfer
  * MOSI / MISO /SCK - the SPI pins are also used for WiFi module communication
  * Green LED - the top LED, in green, will light when the module has connected to an SSID
  * Yellow LED - the bottom LED, in yellow, will blink during data transfer


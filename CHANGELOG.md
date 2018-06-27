## v0.0.2 (TBD)

 * PR #52. Standardizes the sensor API to setup, start, read, stop

## v0.0.1 (2018-06-27)

Initial beta release appropriate for calibration runs. Supports the Honeywell
HPM particulate matter sensor (PM2.5 & PM10) and the Adafruit Si7021 temperature
and humidity breakout. Configurable sample cycle (>= 1 minute), data
collection cycle, and "heartbeat" cycle. Primary suppport is for collection to
SD card, but DO_TRANSMIT_DATA config.h flag supports a crude mechanism for
sending data from a single node to a collector.

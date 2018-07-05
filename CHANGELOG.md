## v0.5 (TBD)
 * increases delay between hpm samples
 * PR #64 converts namespace-based sensor drivers into classes

## v0.4 (2018-07-03)
 * Adds a timeout to STANDBY mode to avoid possible endless standby loops

## v0.3 (2018-07-02)
 * PR #59. Abstracts iteration of sensors into linked-list of sensor configs
 * Adds battery level thresholding power save / recharge mode

## v0.2 (2018-06-27)

 * PR #52. Standardizes the sensor API to setup, start, read, stop
 * Fixes formatting of floats and unsigned longs in output data

## v0.1 (2018-06-27)

Initial beta release appropriate for calibration runs. Supports the Honeywell
HPM particulate matter sensor (PM2.5 & PM10) and the Adafruit Si7021 temperature
and humidity breakout. Configurable sample cycle (>= 1 minute), data
collection cycle, and "heartbeat" cycle. Primary suppport is for collection to
SD card, but DO_TRANSMIT_DATA config.h flag supports a crude mechanism for
sending data from a single node to a collector.

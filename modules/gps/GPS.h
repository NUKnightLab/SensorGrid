#include <Adafruit_GPS.h>
#define GPSSerial Serial1

#ifdef DEBUG
  #ifdef F
    #define DEBUG_PRINT(x)  Serial.print(x)
  #else
    #define DEBUG_PRINT(x)  Serial.print(F(x))
  #endif
#else
 #define DEBUG_PRINT(x)
#endif

Adafruit_GPS GPS(&GPSSerial);

void _readGPS() {
    // Should be called from a timer interrupt. Only reads 1 char at a time!
    char c = GPS.read();
    if (GPS.newNMEAreceived()) {
        // a tricky thing here is if we print the NMEA sentence, or data
        // we end up not listening and catching other sentences!
        // so be very wary if using OUTPUT_ALLDATA and trytng to print out data
        if (!GPS.parse(GPS.lastNMEA()))   // this also sets the newNMEAreceived() flag to false
            return; // we can fail to parse a sentence in which case we should just wait for another
    }
}

#if defined(__AVR_ATmega32U4__)

boolean usingInterrupt_32u4 = true;

void _useInterrupt(boolean v) {
  if (v) {
    // Timer0 is already used for millis() - we'll just interrupt somewhere
    // in the middle and call the "Compare A" function above
    OCR0A = 0xAF;
    TIMSK0 |= _BV(OCIE0A);
    usingInterrupt_32u4 = true;
  } else {
    // do not call the interrupt function COMPA anymore
    TIMSK0 &= ~_BV(OCIE0A);
    usingInterrupt_32u4 = false;
  }
}

SIGNAL(TIMER0_COMPA_vect) {
    _readGPS();
}

void _setupGPSInterrupt() {
    _useInterrupt(usingInterrupt_32u4);
}
#elif defined(ARDUINO_ARCH_SAMD)

/**
 * Generic Clock Multiplexer IDs for M0/SAMD are defined here:
 * https://github.com/arduino/ArduinoCore-samd/blob/13b371ba371aefcd2da9e71b4694d93641eeeaae/cores/arduino/WVariant.h
 *
 * M0 TC0 interrupt code adapted from example by Markus Bader:
 * https://github.com/maxbader/arduino_tools/blob/master/libraries/timer_zero_tcc_interrupt/timer_zero_tcc_interrupt.ino
 */

void _setupGPSInterrupt() {
    REG_GCLK_CLKCTRL = (uint16_t) (GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID_TCC0_TCC1) ;
    while ( GCLK->STATUS.bit.SYNCBUSY == 1 ); // wait for sync
    // The type cast must fit with the selected timer
    Tcc* TC = (Tcc*) TCC0; // get timer struct
    TC->CTRLA.reg &= ~TCC_CTRLA_ENABLE;   // Disable TC
    while (TC->SYNCBUSY.bit.ENABLE == 1); // wait for sync
    TC->CTRLA.reg |= TCC_CTRLA_PRESCALER_DIV256;   // Set perscaler
    TC->WAVE.reg |= TCC_WAVE_WAVEGEN_NFRQ;   // Set wave form configuration
    while (TC->SYNCBUSY.bit.WAVE == 1); // wait for sync
    //TC->PER.reg = 0xFFFF;              // Set counter Top using the PER register
    TC->PER.reg = 0x1111; // This may need some tweaking for a good GPS read
    while (TC->SYNCBUSY.bit.PER == 1); // wait for sync
    TC->CC[0].reg = 0xFFF;
    while (TC->SYNCBUSY.bit.CC0 == 1); // wait for sync
    TC->INTENSET.reg = 0;                 // disable all interrupts
    TC->INTENSET.bit.OVF = 1;          // enable overfollow
    TC->INTENSET.bit.MC0 = 1;          // enable compare match to CC0
    NVIC_EnableIRQ(TCC0_IRQn);
    TC->CTRLA.reg |= TCC_CTRLA_ENABLE ;
    while (TC->SYNCBUSY.bit.ENABLE == 1); // wait for sync
}

void TCC0_Handler(){
    Tcc* TC = (Tcc*) TCC0;       // get timer struct
    if (TC->INTFLAG.bit.OVF == 1) {  // A overflow caused the interrupt
        TC->INTFLAG.bit.OVF = 1;    // writing a one clears the flag ovf flag
    }
    if (TC->INTFLAG.bit.MC0 == 1) {  // A compare to cc0 caused the interrupt
        TC->INTFLAG.bit.MC0 = 1;    // writing a one clears the flag ovf flag
        _readGPS();
    }
}
#else
void _setupGPSInterrupt() {
    Serial.println("WARNING: Unsupported Arduino chipset for use with GPS module ...");
    Serial.println("... Support currently provided for Adafruit Feather M0 & 32u4 Boards");
}
#endif

void setupGPS() {
  DEBUG_PRINT("INIT GPS..");
  GPS.begin(9600);
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);
  _setupGPSInterrupt();
  DEBUG_PRINT("  ..GPS INIT\n"); // TODO: is there a way to tell GPS init is unsuccessful?
  delay(500);
}

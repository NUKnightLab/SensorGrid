#include <Adafruit_GPS.h>
#define GPSSerial Serial1
//#ifndef GPSECHO
//#define GPSECHO false
//#endif

Adafruit_GPS GPS(&GPSSerial);
unsigned long lastGPS = 0;

void _readGPSIfTime() {
  // call this from a timer interrupt
  if (millis() - lastGPS > 15 * 1000) {
        char c = GPS.read();
        if (GPS.newNMEAreceived()) {
            // a tricky thing here is if we print the NMEA sentence, or data
            // we end up not listening and catching other sentences!
            // so be very wary if using OUTPUT_ALLDATA and trytng to print out data
            if (!GPS.parse(GPS.lastNMEA()))   // this also sets the newNMEAreceived() flag to false
                return;  // we can fail to parse a sentence in which case we should just wait for another
            lastGPS = millis();
        }
  }
}

/* crude handling of SIGNAL only available on 32u4 */
#if BOARD == Feather32u4
    boolean usingInterrupt = true; // interrupts not working well at the moment
#else
    boolean usingInterrupt = false;
#endif

/******************************************************************/
// Interrupt is called once a millisecond, looks for any new GPS data, and stores it

/*
 * 
 * This works for 32u4 boards only but not working well right now*/

#if BOARD == Feather32u4

void useInterrupt(boolean v) {
  if (v) {
    // Timer0 is already used for millis() - we'll just interrupt somewhere
    // in the middle and call the "Compare A" function above
    OCR0A = 0xAF;
    TIMSK0 |= _BV(OCIE0A);
    usingInterrupt = true;
  } else {
    // do not call the interrupt function COMPA anymore
    TIMSK0 &= ~_BV(OCIE0A);
    usingInterrupt = false;
  }
}

SIGNAL(TIMER0_COMPA_vect) {
    /*
    char c = GPS.read();   
    if (GPS.newNMEAreceived()) {
         GPS.parse(GPS.lastNMEA());
    } */   
    _readGPSIfTime();
}

#else if BOARD == FeatherM0

/* Generic Clock Multiplexer IDs are defined here:
 *  https://github.com/arduino/ArduinoCore-samd/blob/13b371ba371aefcd2da9e71b4694d93641eeeaae/cores/arduino/WVariant.h
 */

/**
 * From example by Markus Bader:
 * https://github.com/maxbader/arduino_tools/blob/master/libraries/timer_zero_tcc_interrupt/timer_zero_tcc_interrupt.ino
 */
 
int pin_ovf_led = 13; // debug pin for overflow led 
int pin_mc0_led = 5;  // debug pin for compare led 
unsigned int loop_count = 0;
unsigned int irq_ovf_count = 0;

void setupTimer() {

  pinMode(pin_ovf_led, OUTPUT);   // for debug leds
  digitalWrite(pin_ovf_led, LOW); // for debug leds
  pinMode(pin_mc0_led, OUTPUT);   // for debug leds
  digitalWrite(pin_mc0_led, LOW); // for debug leds

  // Enable clock for TC 
  REG_GCLK_CLKCTRL = (uint16_t) (GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID_TCC0_TCC1) ;
  while ( GCLK->STATUS.bit.SYNCBUSY == 1 ); // wait for sync
  // The type cast must fit with the selected timer 
  Tcc* TC = (Tcc*) TCC0; // get timer struct
  TC->CTRLA.reg &= ~TCC_CTRLA_ENABLE;   // Disable TC
  while (TC->SYNCBUSY.bit.ENABLE == 1); // wait for sync 
  TC->CTRLA.reg |= TCC_CTRLA_PRESCALER_DIV256;   // Set perscaler
  TC->WAVE.reg |= TCC_WAVE_WAVEGEN_NFRQ;   // Set wave form configuration 
  while (TC->SYNCBUSY.bit.WAVE == 1); // wait for sync 
  TC->PER.reg = 0xFFFF;              // Set counter Top using the PER register  
  while (TC->SYNCBUSY.bit.PER == 1); // wait for sync 
  TC->CC[0].reg = 0xFFF;
  while (TC->SYNCBUSY.bit.CC0 == 1); // wait for sync 
  // Interrupts 
  TC->INTENSET.reg = 0;                 // disable all interrupts
  TC->INTENSET.bit.OVF = 1;          // enable overfollow
  TC->INTENSET.bit.MC0 = 1;          // enable compare match to CC0
  // Enable InterruptVector
  NVIC_EnableIRQ(TCC0_IRQn);
  // Enable TC
  TC->CTRLA.reg |= TCC_CTRLA_ENABLE ;
  while (TC->SYNCBUSY.bit.ENABLE == 1); // wait for sync 
}


void TCC0_Handler()
{
  Tcc* TC = (Tcc*) TCC0;       // get timer struct
  if (TC->INTFLAG.bit.OVF == 1) {  // A overflow caused the interrupt
    digitalWrite(pin_ovf_led, irq_ovf_count % 2); // for debug leds
    digitalWrite(pin_mc0_led, HIGH); // for debug leds
    TC->INTFLAG.bit.OVF = 1;    // writing a one clears the flag ovf flag
    irq_ovf_count++;                 // for debug leds
  }
  
  if (TC->INTFLAG.bit.MC0 == 1) {  // A compare to cc0 caused the interrupt
    digitalWrite(pin_mc0_led, LOW);  // for debug leds
    TC->INTFLAG.bit.MC0 = 1;    // writing a one clears the flag ovf flag
    _readGPSIfTime();
  }
}
#endif

void setupGPS() {
  Serial.print(F("INIT GPS.."));
  GPS.begin(9600);
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);
  #if BOARD == Feather32u4
    useInterrupt(usingInterrupt);
  #else if BOARD == FeatherM0
    setupTimer();
  #endif
  Serial.println(F("  ..GPS INIT")); // TODO: is there a way to tell GPS init is unsuccessful?
  delay(500);
}






#include <Adafruit_GPS.h>
#define GPSSerial Serial1
#ifndef GPSECHO
#define GPSECHO false
#endif

Adafruit_GPS GPS(&GPSSerial);
unsigned long lastGPS = 0;

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


#endif



/*********************************************/
/* Non-working attempts to handle M0 Signals */
/* This is not working for M0 b/c TC3_Handler is already defined by Radiohead */

// From https://github.com/maxbader/arduino_tools/tree/master/libraries

/**
 * @author Markus Bader
 * @brief this program shows how to use the TC timer with interrupts on an Arduino Zero board
 * @email markus.bader@tuwien.ac.at
 */

/*
//int pin_ovf_led = 13;  // debug pin for overflow led 
//int pin_mc0_led = 5;  // debug pin for compare led 
//unsigned int loop_count = 0;
unsigned int irq_ovf_count = 0;

void setupSignal() {

  //pinMode(pin_ovf_led, OUTPUT);   // for debug leds
  //digitalWrite(pin_ovf_led, LOW); // for debug leds
  //pinMode(pin_mc0_led, OUTPUT);   // for debug leds
  //digitalWrite(pin_mc0_led, LOW); // for debug leds

  
  // Enable clock for TC 
  REG_GCLK_CLKCTRL = (uint16_t) (GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID_TCC2_TC3) ;
  while ( GCLK->STATUS.bit.SYNCBUSY == 1 ); // wait for sync 

  // The type cast must fit with the selected timer mode 
  TcCount16* TC = (TcCount16*) TC3; // get timer struct

  TC->CTRLA.reg &= ~TC_CTRLA_ENABLE;   // Disable TC
  while (TC->STATUS.bit.SYNCBUSY == 1); // wait for sync 

  TC->CTRLA.reg |= TC_CTRLA_MODE_COUNT16;  // Set Timer counter Mode to 16 bits
  while (TC->STATUS.bit.SYNCBUSY == 1); // wait for sync 
  TC->CTRLA.reg |= TC_CTRLA_WAVEGEN_NFRQ; // Set TC as normal Normal Frq
  while (TC->STATUS.bit.SYNCBUSY == 1); // wait for sync 

  TC->CTRLA.reg |= TC_CTRLA_PRESCALER_DIV256;   // Set perscaler
  while (TC->STATUS.bit.SYNCBUSY == 1); // wait for sync 
  
  // TC->PER.reg = 0xFF;   // Set counter Top using the PER register but the 16/32 bit timer counts allway to max  
  // while (TC->STATUS.bit.SYNCBUSY == 1); // wait for sync 

  TC->CC[0].reg = 0xFFF;
  while (TC->STATUS.bit.SYNCBUSY == 1); // wait for sync 
  
  // Interrupts 
  TC->INTENSET.reg = 0;              // disable all interrupts
  TC->INTENSET.bit.OVF = 1;          // enable overfollow
  TC->INTENSET.bit.MC0 = 1;          // enable compare match to CC0

  // Enable InterruptVector
  NVIC_EnableIRQ(TC3_IRQn);

  // Enable TC
  TC->CTRLA.reg |= TC_CTRLA_ENABLE;
  while (TC->STATUS.bit.SYNCBUSY == 1); // wait for sync 

}

void TC3_Handler()
{
  TcCount16* TC = (TcCount16*) TC3; // get timer struct
  if (TC->INTFLAG.bit.OVF == 1) {  // A overflow caused the interrupt
    //digitalWrite(pin_ovf_led, irq_ovf_count % 2); // for debug leds
    //digitalWrite(pin_mc0_led, HIGH); // for debug leds
    Serial.println("OVERFLOW INTERRUPT");
    TC->INTFLAG.bit.OVF = 1;    // writing a one clears the flag ovf flag
    irq_ovf_count++;                 // for debug leds
  }
  
  if (TC->INTFLAG.bit.MC0 == 1) {  // A compare to cc0 caused the interrupt
    //digitalWrite(pin_mc0_led, LOW);  // for debug leds
    Serial.println("COMPARE cc0 interrupt");
    TC->INTFLAG.bit.MC0 = 1;    // writing a one clears the flag ovf flag
  }
}
*/
// /M0 Signal setup


void setupGPS() {
  Serial.println(F("INIT GPS.."));
  GPS.begin(9600);
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);
  #if BOARD == Feather32u4
    useInterrupt(usingInterrupt);
  #endif
  //setupSignal(); // Not currently working for M0
  Serial.println(F("..GPS INIT")); // TODO: is there a way to tell GPS init is unsuccessful?
  delay(500);
}

/*
void printGPS() {
  Serial.println("GPS:");
  Serial.print("    DT: 20");
  Serial.print(GPS.year, DEC); Serial.print('-');
  Serial.print(GPS.month, DEC); Serial.print('-');
  Serial.print(GPS.day, DEC); Serial.print('T');
  Serial.print(GPS.hour, DEC); Serial.print(':');
  Serial.print(GPS.minute, DEC); Serial.print(':');
  Serial.print(GPS.seconds, DEC); Serial.print('.');
  Serial.println(GPS.milliseconds);
  Serial.print("    Fix: "); Serial.print((int)GPS.fix);
  Serial.print("; Quality: "); Serial.println((int)GPS.fixquality);
  if (GPS.fix) {
    Serial.print("    Location: ");
    Serial.print(GPS.latitude, 4); Serial.print(GPS.lat);
    Serial.print(", ");
    Serial.print(GPS.longitude, 4); Serial.println(GPS.lon);
    Serial.print("    Speed (knots): "); Serial.print(GPS.speed);
    Serial.print("; Angle: "); Serial.println(GPS.angle);
    Serial.print("    Altitude: "); Serial.print(GPS.altitude);
    Serial.print("; Satellites: "); Serial.println((int)GPS.satellites);
  }
}
*/

/*
void readGPS() {
  char c = GPS.read();
  if (GPS.newNMEAreceived()) {
    // a tricky thing here is if we print the NMEA sentence, or data
    // we end up not listening and catching other sentences!
    // so be very wary if using OUTPUT_ALLDATA and trytng to print out data
    //Serial.println(GPS.lastNMEA());   // this also sets the newNMEAreceived() flag to false
    GPS.parse(GPS.lastNMEA());
    //if (!GPS.parse(GPS.lastNMEA()))   // this also sets the newNMEAreceived() flag to false
    //  return;  // we can fail to parse a sentence in which case we should just wait for another
    //printGPS(); x
    //lastGPS = millis();
  } else {
    Serial.println("No new NMEA received");
  }
}
*/






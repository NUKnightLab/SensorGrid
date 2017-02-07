#include <Adafruit_GPS.h>
#define GPSSerial Serial1
#ifndef GPSECHO
  #define GPSECHO false
#endif

Adafruit_GPS GPS(&GPSSerial);
unsigned long lastGPS = 0;

boolean usingInterrupt = true;

/******************************************************************/
// Interrupt is called once a millisecond, looks for any new GPS data, and stores it

SIGNAL(TIMER0_COMPA_vect) {
  char c = GPS.read();
  // if you want to debug, this is a good time to do it!
  if (GPSECHO)
    if (c) Serial.print(c);  
}

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
/********************************/

void setupGPS() {
    Serial.println("Initializing GPS");
    GPS.begin(9600);
    GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
    GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);
    GPS.sendCommand(PGCMD_ANTENNA);
    useInterrupt(usingInterrupt);
    Serial.println("GPS initialized");
    delay(500);
}

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

void readGPS() {
    if (! usingInterrupt) {
      char c = GPS.read();
      if (GPSECHO) {
        if (c) Serial.println(c);
      }
    }
    if (GPS.newNMEAreceived()) {
      // a tricky thing here is if we print the NMEA sentence, or data
      // we end up not listening and catching other sentences!
      // so be very wary if using OUTPUT_ALLDATA and trytng to print out data
      //Serial.println(GPS.lastNMEA());   // this also sets the newNMEAreceived() flag to false
      if (!GPS.parse(GPS.lastNMEA()))   // this also sets the newNMEAreceived() flag to false
        return;  // we can fail to parse a sentence in which case we should just wait for another
      printGPS();
      lastGPS = millis();
    }
}





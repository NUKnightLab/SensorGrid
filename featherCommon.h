#define LED 13
#if BOARD == Feather32u4 // not sure why ifdef/ifndef for A7,A9 doesn't work
    #define VBATPIN A9
#else
    #define VBATPIN A7
#endif

#if defined(ARDUINO_SAMD_ZERO) && defined(SERIAL_PORT_USBVIRTUAL)
    // Required for Serial on Zero based boards
    #define Serial SERIAL_PORT_USBVIRTUAL
#endif

class __FlashStringHelper;
#define F(string_literal) (reinterpret_cast<const __FlashStringHelper*>(PSTR(string_literal)))

void flashLED(int times, int endState) {
    digitalWrite(LED, LOW); delay(50);
    for (int i=0; i<times; i++) {
        digitalWrite(LED, HIGH); delay(50);
        digitalWrite(LED, LOW); delay(50);
    }
    digitalWrite(LED, endState);
}

float batteryLevel() {
    float measuredvbat = analogRead(VBATPIN);
    measuredvbat *= 2;    // we divided by 2, so multiply back
    measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
    measuredvbat /= 1024; // convert to voltage
    return measuredvbat;
}

#if BOARD == Feather32u4
int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}
*/

//#include <MemoryFree.h>
void printRam() {
    // Not working on M0;
    //Serial.print(F("freeMemory: "));
    //Serial.println(freeMemory());
    //Serial.print("; ");
    Serial.print(F("freeRam: "));
    Serial.println(freeRam());
}
#else
//TODO: get RAM check working on M0
void printRam(){}
#endif


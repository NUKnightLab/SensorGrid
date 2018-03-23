/*
  *  Use the I2C bus with EEPROM 24LC64
  *  Sketch:    eeprom.ino
  *
  *  Author: hkhijhe
  *  Date: 01/10/2010
  *
  *
  */
#include <time.h>
#include <Wire.h>

//#define MAX_EEPROM_ADDR 0x7FFF
#define MAX_EEPROM_ADDR 100

bool core = true;

void i2c_eeprom_write_byte( int deviceaddress, unsigned int eeaddress, byte data ) {
    int rdata = data;
    Wire.beginTransmission(deviceaddress);
    Wire.write((int)(eeaddress >> 8)); // MSB
    Wire.write((int)(eeaddress & 0xFF)); // LSB
    Wire.write(rdata);
    Wire.endTransmission();
}

// WARNING: address is a page address, 6-bit end will wrap around
// also, data can be maximum of about 30 bytes, because the Wire library has a buffer of 32 bytes
void i2c_eeprom_write_page( int deviceaddress, unsigned int eeaddresspage, byte* data, byte len ) {
    if (len > 30) len = 30;
    byte buf[30] = {};
    memcpy(buf, data, len);
    Wire.beginTransmission(deviceaddress);
    Serial.print("WRITING Addr: "); Serial.println(eeaddresspage, HEX);
    Wire.write((int)(eeaddresspage >> 8)); // MSB
    Wire.write((int)(eeaddresspage & 0xFF)); // LSB
    Wire.write(buf, 30);
    Wire.endTransmission();
    delay(100);
}

void i2c_eeprom_read_page(byte* buf, uint8_t* buflen, int deviceaddress, unsigned int eeaddress) {
    Wire.beginTransmission(deviceaddress);
    Wire.write((int)(eeaddress >> 8)); // MSB
    Wire.write((int)(eeaddress & 0xFF)); // LSB
    Wire.endTransmission();
    Wire.requestFrom(deviceaddress, *buflen);
    uint8_t i = 0;
    while (i< *buflen && Wire.available()) {
        buf[i++] = Wire.read();
    }
    *buflen = i;
}

byte i2c_eeprom_read_byte( int deviceaddress, unsigned int eeaddress ) {
    byte rdata = 0xFF;
    Wire.beginTransmission(deviceaddress);
    Wire.write((int)(eeaddress >> 8)); // MSB
    Wire.write((int)(eeaddress & 0xFF)); // LSB
    Wire.endTransmission();
    Wire.requestFrom(deviceaddress,1);
    if (Wire.available()) rdata = Wire.read();
    return rdata;
}

void i2c_eeprom_read_buffer( int deviceaddress, unsigned int eeaddress, byte *buffer, int length ) {
    Wire.beginTransmission(deviceaddress);
    Wire.write((int)(eeaddress >> 8)); // MSB
    Wire.write((int)(eeaddress & 0xFF)); // LSB
    Wire.endTransmission();
    Wire.requestFrom(deviceaddress,length);
    int c = 0;
    for ( c = 0; c < length; c++ )
        if (Wire.available()) buffer[c] = Wire.read();
}


void clear_data()
{
    /** 
     *  Wire library buffer is 32 bytes. There is 2-bytes addressing overhead. EEPROM page size is 64 bytes so we
     *  clear a page in 3 separate writes. Ideally we would have a Wire buffer of >= 66 bytes in order to clear a whole
     *  page at once.
     */
    byte nodata_22[22] = {};
    byte nodata_20[20] = {};
    int addr = 0;
    while (addr < MAX_EEPROM_ADDR) {
        i2c_eeprom_write_page(0x50, addr, nodata_22, 22);
        i2c_eeprom_write_page(0x50, addr+22, nodata_22, 22);
        i2c_eeprom_write_page(0x50, addr+44, nodata_20, 20);
        addr += 64;
    }
    delay(500);
}

void setup()
{
   srand(time(NULL));
   Wire.begin();
   while (!Serial);
   Serial.begin(9600);
   if (core) {
      //byte testdata[30] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30};
      //byte testdata = {};
      Serial.println("Clearing data ..");
      //clear_data();
      Serial.println(".. cleared");
      Serial.println("Writing test 30-byte half-pages");
      int pagecount = 0;

/*
      for (int pageaddr=0; pageaddr<=0x7FFF; pageaddr+=32) {  
          testdata[0] = pageaddr >> 8;
          testdata[1] = pageaddr & 0xFF;
          i2c_eeprom_write_page(0x50, pageaddr, testdata, 30); // Arduino wire buffer is 32 bytes. 2 bytes EEPROM addr overhead, thus 30
          pagecount++;
          // TODO: can we do ACKnowledged polling? See datasheet p. 11
          delay(100); // max page write time == 500ms
      }
*/
      for (int pageaddr=0; pageaddr<=MAX_EEPROM_ADDR; pageaddr+=32) {
          byte testdata[30] = {};
          int r = (rand() % 29) + 1;
          int index = 1;
          //testdata[index++] = r;
          int sum = 0;
          for (int i=0; i<r; i++) {
              byte b = rand() % 0xFF;
              sum += b;
              testdata[index++] = b;
          }
          testdata[0] = sum;
          //testdata[index] = sum;
          i2c_eeprom_write_page(0x50, pageaddr, testdata, 30);
          pagecount++;
      }
      Serial.print("Test pages written: "); Serial.println(pagecount);
      delay(2000);
      Serial.println("\nReading pages:");
      for (int i=0; i<=MAX_EEPROM_ADDR; i+=32) {
          Serial.print("Addr: "); Serial.print(i>>8, DEC); 
          Serial.print(" "); Serial.print(i&0xFF);
          Serial.print(" Data: ");
          byte buf[30];
          uint8_t buflen = 30;
          i2c_eeprom_read_page(buf, &buflen, 0x50, i);
          int sum = 0;
          for (int j=1; j<buflen; j++) {
              sum += buf[j];
              Serial.print(buf[j], DEC);
              Serial.print(" ");
          }
          Serial.println("");
          if (sum != buf[0]) {
              Serial.print("WARNING: addr: ");
              Serial.print(i, DEC);
              Serial.print(" Sum: "); Serial.print(sum);
              Serial.print(" != "); Serial.println(buf[0]);
          }
          Serial.println("OK");
      }
   }

    //char somedata[] = "this is data from the eeprom"; // data to write
    //i2c_eeprom_write_page(0x50, 0x7FFF, (byte *)somedata, sizeof(somedata)); // write to EEPROM
    //delay(100);
    //i2c_eeprom_write_page(0x50, 1, (byte *)somedata, sizeof(somedata)); // write to EEPROM
    //delay(100); //add a small delay
    //Serial.println("Memory written");

}

void loop()
{
    return;
    static int addr=0;
    char data[32];
    if (core) {
        int len = 1 + sprintf(data, "data at address %d", addr);
        Serial.print("Writing data: ");
        Serial.println(data);
        Serial.print("Of length: "); Serial.println(len);
        if (addr + len > 0x7FFF) addr = 0;
        i2c_eeprom_write_page(0x50, addr, (byte *)data, len);
        addr += len;
        Serial.print("New address: ");
        Serial.println(addr, DEC);
        delay(2000);
    } else {
        addr = 0;
        while (addr <= 0x7FFF) {
            byte b = i2c_eeprom_read_byte(0x50, addr);
            while (b!=0) {
                Serial.print((char)b);
                b = i2c_eeprom_read_byte(0x50, ++addr);
                i2c_eeprom_write_byte(0x50, addr, 0);
            }
            addr++;
            if (b!=0) {
                Serial.println("");
            } 
        }
    }
}

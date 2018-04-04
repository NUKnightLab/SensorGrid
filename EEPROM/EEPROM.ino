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
#define MAX_EEPROM_ADDR 200
bool TEST_ODD_BITS = false;
bool TEST_EVEN_BITS = false;
bool TEST_RANDOM_CHECKSUMS = false;

bool core = false;

void i2c_eeprom_write_byte( int deviceaddress, unsigned int eeaddress, byte data ) {
    int rdata = data;
    Wire.beginTransmission(deviceaddress);
    Wire.write((int)(eeaddress >> 8)); // MSB
    Wire.write((int)(eeaddress & 0xFF)); // LSB
    Wire.write(rdata);
    Wire.endTransmission();
}

/**
 *  WARNING: address is a page address, 6-bit end will wrap around
 *  also, data can be maximum of 30 bytes, because the Wire library has a buffer of 32 bytes
 *  (2 are used for the write address)
 */
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

/**
 * 2 bytes are used for the checksum. Max data length is 28 bytes and will be truncated
 */
void i2c_eeprom_write_checked_page(int deviceaddress, unsigned int eeaddresspage, byte* data, byte len)
{
    if (len > 28) len = 28;
    byte buf[30] = {};
    int sum = 0;
    for (int i=0; i<len; i++) {
        sum += data[i];
    }
    buf[0] = sum>>8;
    buf[1] = sum&0xFF;
    memcpy(buf+2, data, len);
    Wire.beginTransmission(deviceaddress);
    Serial.print("WRITING Addr: "); Serial.println(eeaddresspage, HEX);
    Wire.write((int)(eeaddresspage >> 8)); // MSB
    Wire.write((int)(eeaddresspage & 0xFF)); // LSB
    Wire.write(buf, 30);
    Wire.endTransmission();
    delay(100);
}


void i2c_eeprom_read_page(byte* buf, int8_t* buflen, int deviceaddress, unsigned int eeaddress) {
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

void i2c_eeprom_read_checked_page(byte* buf, int8_t* buflen, int deviceaddress, unsigned int eeaddress) {
    Wire.beginTransmission(deviceaddress);
    Wire.write((int)(eeaddress >> 8)); // MSB
    Wire.write((int)(eeaddress & 0xFF)); // LSB
    Wire.endTransmission();
    Wire.requestFrom(deviceaddress, *buflen);
    int checksum = Wire.read()<<8 | Wire.read()&0xFF;
    int sum = 0;
    uint8_t i = 0;
    while (i< *buflen && Wire.available()) {
        byte b = Wire.read();
        buf[i++] = b;
        sum += b;
    }
    if (sum == checksum) {
        Serial.print("Addr: ");
        Serial.print(eeaddress>>8 | eeaddress&0xFF, DEC);
        Serial.print(" OK. sum: ");
        Serial.print(checksum, DEC);
        *buflen = i;
    } else {
        Serial.print("** WARNING: addr: ");
        Serial.print(eeaddress, DEC);
        Serial.print(" Sum: "); Serial.print(sum, DEC);
        Serial.print(" != "); Serial.print(checksum, DEC);
        *buflen = -1;
    }
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
    delay(500); // Max page write time
}

/**
 * Check the write of a standard value to all bytes of all pages
 */
void test_bytes(byte val)
{
    int pagecount = 0;
    for (int pageaddr=0; pageaddr<=MAX_EEPROM_ADDR; pageaddr+=32) {
        byte testdata[30] = {};
        int index = 0;
        for (int i=0; i<30; i++) {
            testdata[index++] = val;
        }
        i2c_eeprom_write_page(0x50, pageaddr, testdata, 30);
        pagecount++;
    }
    int bad_page_count = 0;
    int bad_byte_count = 0;
    Serial.print("Value (");
    Serial.print(val, DEC);
    Serial.print(") test pages written: "); Serial.println(pagecount);
    delay(2000);
    Serial.println("\nReading pages:");
    for (int i=0; i<=MAX_EEPROM_ADDR; i+=32) {
        int bad_bytes = 0;
        byte buf[30];
        int8_t buflen = 30;
        i2c_eeprom_read_page(buf, &buflen, 0x50, i);
        Serial.print(" Data: ");
        for (int j=0; j<buflen; j++) {
            Serial.print(buf[j], DEC);
            Serial.print(" ");
            if (buf[j] != val) {
                bad_bytes++;
            }
        }
        Serial.println("");
        if (bad_bytes) {
            bad_byte_count += bad_bytes;
            bad_page_count++;
        }
    }
    Serial.print(bad_byte_count, DEC);
    Serial.println(" bad bytes");
    Serial.print(bad_page_count, DEC);
    Serial.println(" bad pages");
}

void test_random_writes()
{
    int pagecount = 0;
    for (int pageaddr=0; pageaddr<=MAX_EEPROM_ADDR; pageaddr+=32) {
        byte testdata[28] = {};
        int r = (rand() % 28) + 1;
        int index = 0;
        int sum = 0;
        for (int i=0; i<r; i++) {
            byte b = rand() % 0xFF;
            sum += b;
            testdata[index++] = b;
        }
        i2c_eeprom_write_checked_page(0x50, pageaddr, testdata, 28);
        pagecount++;
    }
    int bad_page_count = 0;
    Serial.print("Random test pages written: "); Serial.println(pagecount);
    delay(2000);
    Serial.println("\nReading pages:");
    for (int i=0; i<=MAX_EEPROM_ADDR; i+=32) {
        byte buf[30];
        int8_t buflen = 30;
        i2c_eeprom_read_checked_page(buf, &buflen, 0x50, i);
        if (buflen < 0) {
            Serial.println("CORRUPTED DATA");
            bad_page_count++;
        } else {
            Serial.print(" Data: ");
            for (int j=0; j<buflen; j++) {
                Serial.print(buf[j], DEC);
                Serial.print(" ");
            }
            Serial.println("");
        }
    }
    Serial.print(bad_page_count, DEC);
    Serial.println(" bad pages");
}

void read_all_data(bool delete_data=false)
{
    for (int pageaddr=0; pageaddr<=MAX_EEPROM_ADDR; ) {
        byte b = i2c_eeprom_read_byte(0x50, pageaddr);
        if (b) {
            byte buf[30];
            int8_t buflen = 30;
            i2c_eeprom_read_checked_page(buf, &buflen, 0x50, pageaddr);
            Serial.print(" Data: ");
            for (int j=0; j<buflen; j++) {
                Serial.print(buf[j], DEC);
                Serial.print(" ");
            }
            Serial.println("");
            if (delete_data) {
                Serial.print("Deleting page at address: ");
                Serial.println(pageaddr, DEC);
                byte nodata[30] = {};
                i2c_eeprom_write_page(0x50, pageaddr, nodata, 30);
            }
            pageaddr += 32;
        } else {
            Serial.print("Waiting for data at address: ");
            Serial.println(pageaddr, DEC);
            delay(2000);
        }
    }
}

void setup()
{
    randomSeed(analogRead(0));
    Wire.begin();
    while (!Serial);
    Serial.begin(9600);
    if (TEST_ODD_BITS)
        test_bytes(0b10101010);
    if (TEST_EVEN_BITS)
        test_bytes(0b01010101);
    if (TEST_RANDOM_CHECKSUMS)
        test_random_writes();

    randomSeed(100);
    if (core) {
        test_random_writes();
        read_all_data();
    } else {
        while(1) {
            read_all_data(true);
        }
    }
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

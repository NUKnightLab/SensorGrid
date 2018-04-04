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


/* http://www.mathcs.emory.edu/~cheung/Courses/255/Syllabus/1-C-intro/bit-array.html */
#define SetBit(A,k)     ( A[(k/32)] |= (1 << (k%32)) )
#define ClearBit(A,k)   ( A[(k/32)] &= ~(1 << (k%32)) )
#define TestBit(A,k)    ( A[(k/32)] & (1 << (k%32)) )

#define IS_CORE 0

#define MAX_EEPROM_ADDR 0x7FFF
//#define MAX_EEPROM_ADDR 500
const int CYCLE_SIZE = MAX_EEPROM_ADDR / (32 * 4) + 1;
bool TEST_ODD_BITS = false;
bool TEST_EVEN_BITS = false;
bool TEST_RANDOM_CHECKSUMS = false;

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
    Serial.print("WRITING Addr: "); Serial.println(eeaddresspage, DEC);
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
    Serial.print("WRITING Addr: "); Serial.println(eeaddresspage, DEC);
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

void read_cycle_data(byte cycle_id, long* rec)
{
    int offset = (cycle_id % 4) < 2 ? 0 : 32;
    int start_page = (cycle_id % 2) + 1; // first page reserved for meta
    for (int page=start_page; (page*64+offset)<=MAX_EEPROM_ADDR; page+=2) {
        if (!TestBit(rec, page/2)) {
            byte buf[30];
            int8_t buflen = 30;
            i2c_eeprom_read_checked_page(buf, &buflen, 0x50, page*64+offset);
            if (buflen > 0) {
                Serial.print(" New data at addr: ");
                Serial.println(page*64+offset);
                //rec[page/2] = 1;
                SetBit(rec, page/2);
            }
            for (int i=0; i<CYCLE_SIZE; i++) {
                Serial.print(TestBit(rec, i) ? 1 : 0, DEC);
            }
            Serial.println();
        }
    }
}

void generate_random_writes(int cycle_id)
{
    int pagecount = 0;
    int offset = (cycle_id % 4) < 2 ? 0 : 32;
    int start_page = (cycle_id % 2) + 1; // first page reserved for meta
    for (int page=start_page; (page*64+offset)<=MAX_EEPROM_ADDR; page+=2) {
        byte testdata[28] = {};
        int r = (rand() % 28) + 1;
        int index = 0;
        int sum = 0;
        for (int i=0; i<r; i++) {
            byte b = rand() % 0xFF;
            sum += b;
            testdata[index++] = b;
        }
        i2c_eeprom_write_checked_page(0x50, page*64+offset, testdata, 28);
        pagecount++;
        //delay(2000);
    }
    Serial.print("Random test pages written: "); Serial.println(pagecount);
}

void test_random_writes()
{
    generate_random_writes(0);
    Serial.println("\nReading pages:");
    int bad_page_count = 0;
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

int read_all_data(int start_addr)
{
    int pageaddr = start_addr;
    while (pageaddr <= MAX_EEPROM_ADDR) {
        Serial.print("Reading address: ");
        Serial.println(pageaddr, DEC);
        byte b = i2c_eeprom_read_byte(0x50, pageaddr);
        if (b && b!= 0xFF) {
            byte buf[30];
            int8_t buflen = 30;
            i2c_eeprom_read_checked_page(buf, &buflen, 0x50, pageaddr);
            Serial.print(" Data: ");
            for (int j=0; j<buflen; j++) {
                Serial.print(buf[j], DEC);
                Serial.print(" ");
            }
            Serial.println("");
            pageaddr += 32;
        } else {
            return pageaddr;
        }
    }
    return pageaddr;
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

    if (IS_CORE) {
        clear_data();
    }
    //randomSeed(100);
    Serial.print("Cycle size: ");
    Serial.println(CYCLE_SIZE, DEC);
    Serial.println("Ready ...");
}

void loop()
{
    static byte cycle_id = 0;
    if (IS_CORE) {
        Serial.print("Starting new data write cycle: ");
        Serial.println(cycle_id, DEC);
        i2c_eeprom_write_byte(0x50, 0, cycle_id);
        generate_random_writes(cycle_id++);
        delay(5000);
    } else {
        static long rec[CYCLE_SIZE/sizeof(long)] = {};
        static int read_index = 32;
        int current_cycle_id = i2c_eeprom_read_byte(0x50, 0);
        read_cycle_data(cycle_id, rec);
        if (current_cycle_id != cycle_id) {
            cycle_id = current_cycle_id;
            memset(rec, 0, CYCLE_SIZE/sizeof(long));
            Serial.println("---------");
        }
        delay(1000);
    }
}

#include "SensorGrid.h"
#include "config.h"
#include <pt.h>

#define VERBOSE 1
#define USE_LOW_SLOW_RADIO_MODE 0
#define WAIT_SERIAL 1
#define DISPLAY_UPDATE_PERIOD 1000
#define MAX_NODES 20
#define MAX_MESSAGE_SIZE 255
#define RECV_BUFFER_SIZE MAX_MESSAGE_SIZE

/* buttons */
static bool shutdown_requested = false;

/* OLED display */
Adafruit_FeatherOLED display = Adafruit_FeatherOLED();
static uint32_t oled_activated_time = 0;
bool oled_is_on;
uint32_t display_clock_time = 0;

/* LoRa */
RH_RF95 *radio;
static RHMesh* router;
int16_t last_rssi[MAX_NODES];

RTC_PCF8523 rtc;

/*
 * interrupts
 */

void aButton_ISR()
{
    static volatile int aButtonState = 0;
    aButtonState = !digitalRead(BUTTON_A);
    if (aButtonState) {
        oled_is_on = !oled_is_on;
        if (oled_is_on) {
            oled_activated_time = millis();
            display_clock_time = 0; // force immediate update - don't wait for next minute
            updateDisplay();
            displayID();
            updateDisplayBattery();
            shutdown_requested = false;
        } else {
            display.clearDisplay();
            display.display();
        }
    }
}

void bButton_ISR()
{
    static volatile int bButtonState = 0;
    if (shutdown_requested) {
        display.clearDisplay();
        display.setCursor(0,24);
        display.print("Shutdown OK");
        display.display();
        while(1);
    }
}

void cButton_ISR()
{
    static volatile int cButtonState = 0;
    if (shutdown_requested) {
        shutdown_requested = false;
        display.fillRect(0, 24, 128, 29, BLACK);
        display.display();
    } else {
        shutdown_requested = true;
        display.fillRect(0, 24, 128, 29, BLACK);
        display.setCursor(0,24);
        display.print("B:Shutdown   C:Cancel");
        display.display();
    }
}

/*
 * protothreads (https://github.com/fernandomalmeida/protothreads-arduino) 
 */

static struct pt update_clock_protothread;
static struct pt update_display_protothread;
static struct pt update_display_battery_protothread;
static struct pt display_timeout_protothread;
static struct pt send_collection_request_protothread;

static int updateClockThread(struct pt *pt, int interval)
{
  static unsigned long timestamp = 0;
  PT_BEGIN(pt);
  while(1) {
    PT_WAIT_UNTIL(pt, millis() - timestamp > interval );
    int gps_year = GPS.year;
    if (gps_year != 0 && gps_year != 80) {
        uint32_t gps_time = DateTime(GPS.year, GPS.month, GPS.day, GPS.hour,
            GPS.minute, GPS.seconds).unixtime();
        uint32_t rtc_time = rtc.now().unixtime();
        if (rtc_time - gps_time > 1 || gps_time - rtc_time > 1) {
            rtc.adjust(DateTime(GPS.year, GPS.month, GPS.day, GPS.hour,
                GPS.minute, GPS.seconds));
        }
    }
    timestamp = millis();
  }
  PT_END(pt);
}

static int update_display_thread(struct pt *pt, int interval)
{
  static unsigned long timestamp = 0;
  static uint8_t battery_refresh_count = 0;
  PT_BEGIN(pt);
  while(1) {
    PT_WAIT_UNTIL(pt, millis() - timestamp > interval );
    if ( config.display_timeout > 0
            && (millis() - oled_activated_time) > config.display_timeout*1000) {
        oled_is_on = false;
        shutdown_requested = false;
        display.clearDisplay();
        display.display();
    }
    if (oled_is_on) {
        if (++battery_refresh_count > 30) {
            battery_refresh_count = 0;
            updateDisplayBattery();
        }
        updateDisplay();
    }
    timestamp = millis();
  }
  PT_END(pt);
}

static int send_collection_request_thread(struct pt *pt, int interval)
{
  static unsigned long timestamp = 0;
  PT_BEGIN(pt);
  while(1) {
    PT_WAIT_UNTIL(pt, millis() - timestamp > interval );
    output("\n");
    p(F("Sending collection request\n"));
    uint8_t data[] = { 1,2,3,4};
    uint8_t len = sizeof(data);
    send_message(data, len, 2);
    timestamp = millis();
  }
  PT_END(pt);
}

/* Message utils */

void print_message(Message *msg)
{
    for (int i=0; i<msg->len; i++) {
        p(F("%d "), msg->data[i]);
    }
}

/*
 * Radio I/O
 */

static uint8_t recv_buf[RECV_BUFFER_SIZE] = {0};
static bool recv_buffer_avail = true;

void lock_recv_buffer()
{
    recv_buffer_avail = false;
}

void release_recv_buffer()
{
    recv_buffer_avail = true;
}

bool receive_message(uint8_t* buf, uint8_t* len=NULL, uint8_t* source=NULL,
        uint8_t* dest=NULL, uint8_t* id=NULL, uint8_t* flags=NULL)
{
    if (len == NULL) {
        uint8_t _len;
        len = &_len;
    }
    *len = RECV_BUFFER_SIZE;
    if (!recv_buffer_avail) {
        p(F("WARNING: Could not initiate receive message. Receive buffer is locked.\n"));
        return false;
    }
    Message* _msg;
    lock_recv_buffer(); // lock to be released by calling client
    if (router->recvfromAck(recv_buf, len, source, dest, id, flags)) {
        output("\n");
        _msg = (Message*)recv_buf;
        if ( _msg->sensorgrid_version == config.sensorgrid_version
                && _msg->network_id == config.network_id && _msg->timestamp > 0) {
            uint32_t msg_time = DateTime(_msg->timestamp).unixtime();
            uint32_t rtc_time = rtc.now().unixtime();
            if (rtc_time - msg_time > 1 || msg_time - rtc_time > 1) {
                rtc.adjust(msg_time);
            }
        }
        if ( VERBOSE
             && _msg->sensorgrid_version != config.sensorgrid_version ) {
            p(F("IGNORING: Received message with wrong firmware version: %d\n"),
            _msg->sensorgrid_version);
            return MESSAGE_TYPE_WRONG_VERSION;
        }
        if ( VERBOSE
            && _msg->network_id != config.network_id ) {
            p(F("IGNORING: Received message from wrong network: %d\n"),
            _msg->network_id);
            return MESSAGE_TYPE_WRONG_NETWORK;
        }
        p(F("Received buffered message. LEN: %d; FROM: %d; RSSI: %d; DATA "),
            *len, *source, radio->lastRssi());
        for (int i=0; i<*len; i++) output(F("%d "), _msg->data[i]);
        output(F("\n"));
        last_rssi[*source] = radio->lastRssi();
        return true;
    } else {
        return false;
    }
} /* receive_message */

bool receive(uint8_t* len=NULL, uint8_t* source=NULL,
        uint8_t* dest=NULL, uint8_t* id=NULL, uint8_t* flags=NULL)
{
    return receive_message(recv_buf, len, source, dest, id, flags);
} /* receive */

void check_message()
{
    static bool listening = false;
    static uint16_t count = 0;
    uint8_t from, dest, msg_id, len;
    bool received = receive(&len, &from, &dest, &msg_id);
    unsigned long receive_time = millis();
    if (!listening) {
        listening = true;
        p(F("*** Radio active listen ....\n"));
    }
    if (!++count) output(F("."));
    Message *_msg = (Message*)recv_buf;
    print_message(_msg);
    release_recv_buffer();
} /* check_incoming_message */

uint8_t send_message(uint8_t* data, uint8_t len, uint8_t dest)
{
    p(F("Sending message LEN: %d; DATA: "), len);
    for (int i=0; i<len; i++) output(F("%d "), data[i]);
    output(F("\n"));
    Message msg = {
        .sensorgrid_version=config.sensorgrid_version,
        .network_id=config.network_id,
        .timestamp=millis(),
        .len=len,
    };
    memcpy(msg.data, data, len);
    unsigned long start = millis();
    msg.timestamp = rtc.now().unixtime();
    uint8_t err = router->sendtoWait((uint8_t*)&msg, len, dest);
    p(F("Time to send: %d\n"), millis() - start);
    if (err == RH_ROUTER_ERROR_NONE) {
        output(F("sent\n"));
        return err;
    } else if (err == RH_ROUTER_ERROR_INVALID_LENGTH) {
        p(F("ERROR sending message to Node ID: %d. INVALID LENGTH\n"), dest);
        return err;
    } else if (err == RH_ROUTER_ERROR_NO_ROUTE) {
        p(F("ERROR sending message to Node ID: %d. NO ROUTE\n"), dest);
        return err;
    } else if (err == RH_ROUTER_ERROR_TIMEOUT) {
        p(F("ERROR sending message to Node ID: %d. TIMEOUT\n"), dest);
        return err;
    } else if (err == RH_ROUTER_ERROR_NO_REPLY) {
        p(F("ERROR sending message to Node ID: %d. NO REPLY\n"), dest);
        return err;
    } else if (err == RH_ROUTER_ERROR_UNABLE_TO_DELIVER) {
        p(F("ERROR sending message to Node ID: %d. UNABLE TO DELIVER\n"), dest);
        return err;
    } else {
        p(F("ERROR sending message to Node ID: %d. UNKOWN ERROR CODE: %d\n"), dest, err);
        return err;
    }
} /* send_message */

/*
 * setup and loop
 */

void check_radiohead_version()
{
    p(F("Setting up radio with RadioHead Version %d.%d\n"),
        RH_VERSION_MAJOR, RH_VERSION_MINOR);
    /* TODO: Can RH version check be done at compile time? */
    if (RH_VERSION_MAJOR != REQUIRED_RH_VERSION_MAJOR
        || RH_VERSION_MINOR != REQUIRED_RH_VERSION_MINOR) {
        p(F("ABORTING: SensorGrid requires RadioHead version %d.%d\n"),
            REQUIRED_RH_VERSION_MAJOR, REQUIRED_RH_VERSION_MINOR);
        p(F("RadioHead %d.%d is installed\n"),
            RH_VERSION_MAJOR, RH_VERSION_MINOR);
        while(1);
    }
}

void setup()
{
    if (WAIT_SERIAL)
        while (!Serial);
    rtc.begin();
    check_radiohead_version();
    loadConfig();
    p(F("Node ID: %d\n"), config.node_id);
    p(F("Node type: %d\n"), config.node_type);
    pinMode(LED, OUTPUT);
    pinMode(config.RFM95_RST, OUTPUT);
    pinMode(BUTTON_A, INPUT_PULLUP);
    pinMode(BUTTON_B, INPUT_PULLUP);
    pinMode(BUTTON_C, INPUT_PULLUP);
    digitalWrite(LED, LOW);
    digitalWrite(config.RFM95_RST, HIGH);
    if (config.has_oled) {
        p(F("Display timeout set to: %d\n"), config.display_timeout);
        setupDisplay();
        oled_is_on = true;
        attachInterrupt(BUTTON_A, aButton_ISR, CHANGE);
        attachInterrupt(BUTTON_B, bButton_ISR, FALLING);
        attachInterrupt(BUTTON_C, cButton_ISR, FALLING);
    }
    radio = new RH_RF95(config.RFM95_CS, config.RFM95_INT);
    router = new RHMesh(*radio, config.node_id);
    if (USE_LOW_SLOW_RADIO_MODE)
        radio->setModemConfig(RH_RF95::Bw125Cr48Sf4096);
    if (!router->init()) p(F("Router init failed\n"));
    p(F("FREQ: %.2f\n"), config.rf95_freq);
    if (!radio->setFrequency(config.rf95_freq)) {
        p(F("Radio frequency set failed\n\n"));
    }
    radio->setTxPower(config.tx_power, false);
    radio->setCADTimeout(CAD_TIMEOUT);
    router->setTimeout(TIMEOUT);
    p(F("Setup complete\n"));
    delay(100);
}

void loop()
{
    check_message();
    if (config.has_oled) {
        update_display_thread(&update_display_protothread, DISPLAY_UPDATE_PERIOD);
    }
    if (config.node_type == NODE_TYPE_ORDERED_COLLECTOR) {
        send_collection_request_thread(&send_collection_request_protothread,
            config.collection_period * 1000);
    }
}

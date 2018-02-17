#include "SensorGrid.h"
#include "config.h"
#include <pt.h>

#define VERBOSE 1
#define USE_LOW_SLOW_RADIO_MODE 1
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
#if defined(USE_MESH_ROUTER)
static RHMesh* router;
#else
static RHRouter* router;
#endif
int16_t last_rssi[MAX_NODES];

RTC_PCF8523 rtc;

/* collection state */
static uint8_t known_nodes[] = { 2, 3, 4 };
static uint8_t last_record_ids[MAX_NODES] = {0};

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
    send_data_collection_request();
    timestamp = millis();
  }
  PT_END(pt);
}

/* Message utils */

void print_message(Message *msg, uint8_t len)
{
    uint8_t datalen = len - sizeof(Message);
    for (int i=0; i<datalen; i++) {
        output(F("%d "), msg->data[i]);
    }
    output(F("\n"));
}

void process_data_collection(Message* msg, uint8_t datalen)
{
}

struct Record {
    uint8_t record_type;
    uint8_t data[];
};

struct NodeMessage {
    uint8_t node_id;
    uint8_t max_record_id;
    uint8_t record_count;
    Record records[];
};

void get_preferred_routing_order(uint8* nodes, uint8_t len, uint8_t* order)
{
    uint8_t first_pref[MAX_DATA_RECORDS] = {0};
    uint8_t first_pref_index = 0;
    uint8_t second_pref[MAX_DATA_RECORDS] = {0};
    uint8_t second_pref_index = 0;
    uint8_t third_pref[MAX_DATA_RECORDS] = {0};
    uint8_t third_pref_index = 0;
    for (int i=0; i<len; i++) {
        uint8_t node_id = nodes[i];
        p(F("Checking node ID: %d\n"), node_id);
        if (node_id == config.node_id) {
            p(F("Skipping self ID for preferred routing\n"));
        } else {
            RHRouter::RoutingTableEntry* route = router->getRouteTo(node_id);
            if (route->state == RHRouter::Valid) {
                if (route->next_hop == node_id) {
                    first_pref[first_pref_index++] = node_id;
                    p(F("Node is single hop to ID: %d\n"), node_id);
                } else {
                    second_pref[second_pref_index++] = node_id;
                    p(F("Node is multihop to: %d\n"), node_id);
                }
            } else {
                third_pref[third_pref_index++] = node_id;
                p(F("No known route to ID: %d\n"), node_id);
            }
        }
    }
    p(F("First pref: "));
    for (int i=0; i<MAX_DATA_RECORDS && first_pref[i] > 0; i++) {
        output(F("%d "), first_pref[i]);
    }
    output("\n");
    p(F("Second pref: "));
    for (int i=0; i<MAX_DATA_RECORDS && second_pref[i] > 0; i++) {
        output(F("%d "), second_pref[i]);
    }
    output("\n");
    p(F("Third pref:"));
    for (int i=0; i<MAX_DATA_RECORDS && third_pref[i] > 0; i++) {
        output(F(" %d"), third_pref[i]);
    }
    output("\n");
    memcpy(first_pref+first_pref_index, second_pref, second_pref_index);
    memcpy(first_pref+first_pref_index+second_pref_index, third_pref, third_pref_index);
    p(F("Determined preferred routing: [ "));
    for (int i=0; i<MAX_DATA_RECORDS && first_pref[i] > 0; i++) {
        output(F("%d "), first_pref[i]);
    }
    output(F("]\n"));
    memcpy(order, first_pref, MAX_DATA_RECORDS);
}

const int MAX_NODE_MESSAGES = MAX_MESSAGE_SIZE / sizeof(NodeMessage);

void node_process_message(Message* msg, uint8_t len, uint8_t from)
{
    uint8_t datalen = len - sizeof(Message);
    if (datalen < 3) return;
    uint8_t* data = msg->data;
    uint8_t new_data[MAX_MESSAGE_SIZE - sizeof(Message)];
    static uint8_t node_id;
    static uint8_t max_record_id;
    static uint8_t record_count;
    static uint8_t data_type;
    uint8_t index = 0;
    uint8_t new_data_index = 0;
    uint8_t next_nodes[MAX_NODES] = {0};
    uint8_t next_nodes_index = 0;
    uint8_t collector;
    while (index < datalen) {
        if (!node_id) {
            node_id = data[index++];
            new_data[new_data_index++] = node_id;
        } else if (!max_record_id) {
            max_record_id = data[index++];
            new_data[new_data_index++] = max_record_id;
        } else if (!record_count) {
            record_count = data[index++];
            new_data[new_data_index++] = record_count;
        } else {
            p(F("Message from NODE_ID: %d; MAX_RECORD_ID: %d; RECORD_COUNT: %d\n"),
                node_id, max_record_id, record_count);
            for (int record=0; record<record_count; record++) {
                data_type = data[index++];
                new_data[new_data_index++] = data_type;
                switch (data_type) {
                    case DATA_TYPE_NODE_COLLECTION_LIST :
                        p(F("COLLECTION LIST: "));
                        collector = node_id;
                        uint8_t node_count_index = new_data_index++;
                        new_data[node_count_index] = 0;
                        uint8_t node_count = data[index++];
                        for (int i=0; i<node_count; i++) {
                            uint8_t node = data[index++];
                            uint8_t max_record = data[index++];
                            output(F("NODE_ID: %d; MAX_RECORD_ID: %d; "),
                            node, max_record);
                            if (node != config.node_id) {
                                new_data[new_data_index++] = node;
                                new_data[new_data_index++] = max_record;
                                new_data[node_count_index]++;
                                next_nodes[next_nodes_index++] = node;
                            }
                        }
                        output(F("\n"));
                }
            }
            node_id = 0;
            max_record_id = 0;
            record_count = 0;
        }
    }
    p(F("New data: [ "));
    for (int i=0; i<new_data_index; i++) output(F("%d "), new_data[i]);
    output(F("]\n"));
    uint8_t ordered_nodes[next_nodes_index];
    get_preferred_routing_order(next_nodes, next_nodes_index, ordered_nodes);
    for (int i=0; i<next_nodes_index; i++) {
#if defined(USE_MESH_ROUTER)
        if (RH_ROUTER_ERROR_NONE == send_data(new_data, new_data_index, ordered_nodes[i]))
            return;
#else
        bool trial = false;
        if (router->getRouteTo(ordered_nodes[i])->state != RHRouter::Valid) {
            router->addRouteTo(ordered_nodes[i], ordered_nodes[i]);
            trial = true;
        }
        if (RH_ROUTER_ERROR_NONE == send_data(new_data, new_data_index, ordered_nodes[i])) {
            return;
        } else {
            if (trial) {
                router->deleteRouteTo(ordered_nodes[i]);
            }
        }
#endif
    }

    /* send to the collector if all other nodes fail */
    if (collector) {
        if (router->getRouteTo(collector)->state != RHRouter::Valid) {
            router->addRouteTo(collector, from);
        }
        send_data(new_data, new_data_index, collector);
    }

    router->printRoutingTable();
}

void process_message(Message* msg, uint8_t len, uint8_t from) {
    if (config.node_type == NODE_TYPE_ORDERED_COLLECTOR) {
        p("COLLECTOR DATA RECEIVED\n");
    } else if (config.node_type == NODE_TYPE_ORDERED_SENSOR_ROUTER) {
        node_process_message(msg, len, from);
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
        if (_msg->sensorgrid_version != config.sensorgrid_version ) {
            p(F("IGNORING: Received message with wrong firmware version: %d\n"),
            _msg->sensorgrid_version);
            return false;
        }
        if (_msg->network_id != config.network_id ) {
            p(F("IGNORING: Received message from wrong network: %d\n"),
            _msg->network_id);
            return false;
        }
        p(F("Received message. LEN: %d; FROM: %d; RSSI: %d; DATA "),
            *len, *source, radio->lastRssi());
        uint8_t datalen = *len - sizeof(Message);
        for (int i=0; i<datalen; i++) output(F("%d "), _msg->data[i]);
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
    if (!listening) {
        listening = true;
        p(F("*** Radio active listen ....\n"));
    }
    if (!++count) output(F("."));
    if (receive(&len, &from, &dest, &msg_id)) {
        unsigned long receive_time = millis();
        Message *_msg = (Message*)recv_buf;
        process_message(_msg, len, from);
    }
    release_recv_buffer();
} /* check_incoming_message */

uint8_t send_data(uint8_t* data, uint8_t len, uint8_t dest)
{
    p(F("Sending data LEN: %d; DATA: "), len);
    for (int i=0; i<len; i++) output(F("%d "), data[i]);
    output(F("\n"));
    static struct Message *msg = NULL;
    if (!msg) msg = (Message*)malloc(MAX_MESSAGE_SIZE);
    msg->sensorgrid_version = config.sensorgrid_version;
    msg->network_id = config.network_id;
    memcpy(msg->data, data, len);
    unsigned long start = millis();
    msg->timestamp = rtc.now().unixtime();
    uint8_t err = router->sendtoWait((uint8_t*)msg, len+sizeof(Message), dest);
    p(F("Time to send: %d\n"), millis() - start);
    if (err == RH_ROUTER_ERROR_NONE) {
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
} /* send_data */

void send_data_collection_request()
{
    static uint8_t msg_id = 0;
    const uint8_t node_count = sizeof(known_nodes);
    uint8_t data[5 + node_count*2] = {
        config.node_id,                 // Byte 0
        ++msg_id,                       // 1
        1,                              // 2
        DATA_TYPE_NODE_COLLECTION_LIST, // 3
        node_count                      // 4
    };                                  // --> 5 preliminary bytes total
    uint8_t data_index = 5;
    for (int i=0; i<node_count; i++) {
        data[data_index++] = known_nodes[i];
        data[data_index++] = last_record_ids[known_nodes[i]];
    }
    uint8_t dest = known_nodes[0];
    uint8_t ordered_nodes[node_count];
    /* get_prefered_routing_order is clobbering data! Why?!!!
       using known_nodes for now instead */
    //get_preferred_routing_order(known_nodes, node_count, ordered_nodes);
    for (int i=0; i<node_count; i++) {
        uint8_t node_id = known_nodes[i]; // until get_preferred is working
#if defined(USE_MESH_ROUTER)
        if (RH_ROUTER_ERROR_NONE == send_data(data, data_index, node_id))
            return;
#else
        p(F("Sending DATA: "));
        for (int j=0; j<data_index; j++) output(F("%d "), data[j]);
        bool trial = false;
        if (router->getRouteTo(node_id)->state != RHRouter::Valid) {
            router->addRouteTo(node_id, node_id);
            trial = true;
        }
        if (RH_ROUTER_ERROR_NONE == send_data(data, data_index, node_id)) {
            return;
        } else {
            if (trial) {
                p(F("Removing unsuccessful trial route to: %d\n"), node_id);
                router->deleteRouteTo(node_id);
            }
        }
    }
#endif

} /* send_data_collection_request */

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
#if defined(USE_MESH_ROUTER)
    router = new RHMesh(*radio, config.node_id);
#else
    router = new RHRouter(*radio, config.node_id);
#endif
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

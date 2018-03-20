#include "SensorGrid.h"
#include "config.h"
#include <pt.h>
#include <assert.h>

#define __ASSERT_USE_STDERR
#define VERBOSE 1
#define USE_SLOW_RELIABLE_MODE 0
#define WAIT_SERIAL 0
#define DISPLAY_UPDATE_PERIOD 1000
#define MAX_NODES 20
#define MAX_MESSAGE_SIZE 200
#define RECV_BUFFER_SIZE MAX_MESSAGE_SIZE

static const int COLLECTION_BUFFER_SIZE = 10000;
static uint8_t collection_buffer[COLLECTION_BUFFER_SIZE];
static int collection_buffer_index = 0;


/* TODO: this size is reduced for development purposes */
static const uint8_t MAX_DATA_LENGTH = MAX_MESSAGE_SIZE - sizeof(Message) - 100;

/* buttons */
static bool shutdown_requested = false;

/* OLED display */
Adafruit_FeatherOLED display = Adafruit_FeatherOLED();
static uint32_t oled_activated_time = 0;
bool oled_is_on;
uint32_t display_clock_time = 0;

/* Collection state */
//static uint8_t known_nodes[] = { 2, 3, 4 };
static uint8_t known_nodes[] = { 2, 4, 6 };
static unsigned long next_activity_time = 0;

/* Sensor data */
#define HISTORICAL_DATA_SIZE 20
//static Data historical_data[256];
/* TODO: make historical data a struct */
static Data historical_data[HISTORICAL_DATA_SIZE];
static uint8_t historical_data_head = 0;
static uint8_t historical_data_index = 0;
static uint8_t historical_data_shift_offset = 0;
static uint8_t data_id = 0;

/* Data type structs */

typedef struct __attribute__((packed)) CollectNodeStruct
{
    uint8_t node_id;
    uint8_t prev_record_id;
};

typedef struct __attribute__((packed)) COLLECTION_LIST_STRUCT
{
    uint8_t node_count;
    CollectNodeStruct nodes[MAX_NODES];
};

typedef struct __attribute__((packed)) NEXT_ACTIVITY_SECONDS_STRUCT
{
    uint16_t value;
};

typedef struct __attribute__((packed)) BATTERY_LEVEL_STRUCT
{
    uint8_t value;
};

typedef struct __attribute__((packed)) SHARP_GP2Y1010AU0F_STRUCT
{
    uint16_t value;
    uint32_t timestamp;
};

typedef struct __attribute__((packed)) WARN_50_PCT_DATA_HISTORY_STRUCT
{
};

typedef struct __attribute__((packed)) DataRecord
{
    uint8_t type;
    union {
        COLLECTION_LIST_STRUCT collection_list;
        NEXT_ACTIVITY_SECONDS_STRUCT next_activity_seconds;
        BATTERY_LEVEL_STRUCT battery_level;
        SHARP_GP2Y1010AU0F_STRUCT sharp_gp2y1010au0f;
        WARN_50_PCT_DATA_HISTORY_STRUCT warn_50_pct_data_history;
    };
};

typedef struct __attribute__((packed)) RecordSet
{
    uint8_t node_id;
    uint8_t message_id;
    uint8_t record_count;
    DataRecord records[255];
};


void clean_history(uint8_t acked_message_id)
{
    while (historical_data_head < historical_data_index) {
        if (historical_data[historical_data_head].id <= acked_message_id) {
            historical_data_head++;
        } else {
            break;
        }
    }
}

uint8_t sizeof_record_set(RecordSet* record_set)
{
    uint8_t size = 4; /* node_id + messge_id + record_count + type = 4 bytes */
    for (int i=0; i< record_set->record_count; i++) {
        DataRecord record = record_set->records[i];
        switch (record.type) {
            case DATA_TYPE_NEXT_ACTIVITY_SECONDS :
            {
                size += sizeof(NEXT_ACTIVITY_SECONDS_STRUCT);
                break;
            }
            case DATA_TYPE_NODE_COLLECTION_LIST :
            {
                uint8_t node_count = record.collection_list.node_count;
                size += 1 + 2 * node_count;
                break;
            }
            case DATA_TYPE_SHARP_GP2Y1010AU0F :
            {
                size += sizeof(SHARP_GP2Y1010AU0F_STRUCT);
                break;
            }
            case 7 :
                break;
        }
    }
    return size;
}


/* LoRa */
RH_RF95 *radio;
#if defined(USE_MESH_ROUTER)
/* Mesh routing is no longer officially supported. This option is kept here for
   experimental and development purposes, but support for mesh routing is
   fully deprecated. We will instead be working to make static routing as
   straightforward as possible. The arping and route establishment process of
   RadioHead's mesh router is too heavy, relying too much on repeated
   beaconization and messaging retries. It also aggressively drops routes on
   failure, causing persistent thrashing of route definitions. Also, there is a
   logic error in the route establishment protocol that causes it to always drop
   packets when first establishing the route and only successfully delivering
   them in subsequent transmissions, provided the route has not been otherwise
   dropped in the mean time. */
static RHMesh* router;
#else
static RHRouter* router;
#endif
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
//static struct pt update_display_protothread;
//static struct pt update_display_battery_protothread;
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

void update_display()
{
    static uint8_t battery_refresh_count = 0;
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

/*
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
*/

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
        if (node_id == config.node_id) {
            // p(F("Skipping self ID for preferred routing\n"));
        } else {
            RHRouter::RoutingTableEntry* route = router->getRouteTo(node_id);
            if (route->state == RHRouter::Valid) {
                if (route->next_hop == node_id) {
                    first_pref[first_pref_index++] = node_id;
                } else {
                    second_pref[second_pref_index++] = node_id;
                }
            } else {
                third_pref[third_pref_index++] = node_id;
            }
        }
    }
    memcpy(first_pref+first_pref_index, second_pref, second_pref_index);
    memcpy(first_pref+first_pref_index+second_pref_index, third_pref, third_pref_index);
    p(F("Determined preferred routing: [ "));
    for (int i=0; i<MAX_DATA_RECORDS && first_pref[i] > 0; i++) {
        output(F("%d "), first_pref[i]);
    }
    output(F("]\n"));
    memcpy(order, first_pref, MAX_DATA_RECORDS);
}

//const int MAX_NODE_MESSAGES = MAX_MESSAGE_SIZE / sizeof(NodeMessage);

uint8_t send_data(uint8_t* data, uint8_t len, uint8_t dest, uint8_t flags=0)
{
    p(F("\nSending data to %d; LEN: %d; FLAGS: %d; DATA: "), dest, len, flags);
    for (int i=0; i<len; i++) output(F("%d "), data[i]);
    output(F("\n"));
    static struct Message *msg = NULL;
    if (!msg) msg = (Message*)malloc(MAX_MESSAGE_SIZE);
    msg->sensorgrid_version = config.sensorgrid_version;
    msg->network_id = config.network_id;
    memcpy(msg->data, data, len);
    unsigned long start = millis();
    msg->timestamp = rtc.now().unixtime();
    uint8_t err = router->sendtoWait((uint8_t*)msg, len+sizeof(Message), dest, flags);
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


void node_process_message(Message* message, uint8_t len, uint8_t from)
{
    uint8_t datalen = len - sizeof(Message);
    p(F("Node is process message with datalen: %d\n"), datalen);
    static uint8_t data[MAX_DATA_LENGTH];

    /* If incoming message is a next-activity control, simply re-broadast and wait */
    NewRecordSet* first_set = (NewRecordSet*)message->data;
    if (first_set->data[0] == DATA_TYPE_NEXT_ACTIVITY_SECONDS) {
        _next_activity_seconds* record = (_next_activity_seconds*)first_set->data;
        if (RH_ROUTER_ERROR_NONE == send_data(message->data, datalen, RH_BROADCAST_ADDRESS)) {
            next_activity_time = millis() + 1000 * (record->value);
            return;
        } else {
            p(F("WARNING: FAILED to re-broadcast NEXT_ACTIVITY_SECONDS\n"));
        }
    }
    /* else: this *should* be a collection request */

    /* print out incoming message */
    uint8_t incoming_datalen = datalen;
    uint8_t in_data_index = 0;
    uint8_t in_record_set_size = 0;
    uint8_t size;
    p("** Incoming message:\n");
    print_records(message->data, datalen);
    p("**\n\n");
    /* */

    uint8_t acknowledged_message_id = copy_data(
        data, (uint8_t*)message->data, &datalen, config.node_id);
    clean_history(acknowledged_message_id);
    print_records(data, datalen);

    /* record set for this node */
    //static uint8_t msg_id = 0;

    p("datalen: %d\n", datalen);
    for (int i=0; i<datalen; i++) output("%d ", data[i]);
    output("\n");
    NewRecordSet* set = (NewRecordSet*)&data[datalen];
    set->node_id = config.node_id;
    //set->message_id = ++msg_id;
    set->message_id = 0;
    set->record_count = 0;
    p("Added record set\n");
    for (int i=0; i<datalen+3; i++) output("%d ", data[i]);
    output("\n");

    uint8_t data_size = MAX_DATA_LENGTH - (datalen + 3);

    uint8_t index = 0;
    // battery
    if ((index + sizeof(_BATTERY_LEVEL)) < data_size) {
        _BATTERY_LEVEL* bat = (_BATTERY_LEVEL*)&(set->data[index]);
        bat->type = DATA_TYPE_BATTERY_LEVEL;
        bat->value = (uint8_t)(roundf(batteryLevel() * 10));
        index += sizeof(_BATTERY_LEVEL);
        set->record_count++;
    }
    p("added battery. index: %d\n", index);

    if ((index + sizeof(_WARN_50_PCT_DATA_HISTORY)) < data_size) {
        if ( (historical_data_index - historical_data_head) > HISTORICAL_DATA_SIZE / 2 ) {
            _WARN_50_PCT_DATA_HISTORY* warn = (_WARN_50_PCT_DATA_HISTORY*)&(set->data[index]);
            warn->type = DATA_TYPE_WARN_50_PCT_DATA_HISTORY;
            index += sizeof(_WARN_50_PCT_DATA_HISTORY);
            set->record_count++;
        }
    }

    bool has_more_data = historical_data_index > historical_data_head;
    for (int i=historical_data_head; i<historical_data_index
                && index + sizeof(_SHARP_GP2Y1010AU0F) < data_size; i++) {
        p(F("Setting data record from historical index: %d\n"), i);
        _SHARP_GP2Y1010AU0F* sharp = (_SHARP_GP2Y1010AU0F*)&(set->data[index]);
        *sharp = {
            .type = historical_data[i].type,
            .value = historical_data[i].value,
            .timestamp = historical_data[i].timestamp
        };
        if (historical_data[i].id > set->message_id) {
            set->message_id = historical_data[i].id;
        }
        index += sizeof(_SHARP_GP2Y1010AU0F);
        set->record_count++;
        if (i == historical_data_index - 1) {
            p(F("No more historical data\n"));
            has_more_data = false;
        }
    }

    /* print outgoing message */
    uint8_t outgoing_datalen = datalen + sizeof(NewRecordSet) + index;
    p("outgoing datalen: %d\n", outgoing_datalen);
    for (int i=0; i<outgoing_datalen; i++) output("%d ", data[i]);
    output("\n");
    uint8_t out_data_index = 0;
    uint8_t out_record_set_size = 0;
    p("** Outgoing message:\n");
    print_records(data, outgoing_datalen);
    p("**\n\n");
    /* */

    NewRecordSet* collection_record = (NewRecordSet*)data;
    _COLLECTION_LIST* list = (_COLLECTION_LIST*)collection_record->data;
    uint8_t total_datalen = datalen + sizeof(NewRecordSet) + index;

    if (!has_more_data && index < MAX_DATA_LENGTH - 10) {
        // only forward if we can fit at least a record
        p("next nodes: ");
        for (int i=0; i<list->node_count; i++) output("%d ", list->nodes[i].node_id);
        output("\n");
        for (int i=0; i<list->node_count; i++) {
            uint8_t to_node_id = list->nodes[i].node_id;
            p(F("Attempting forward to: %d\n"), to_node_id);
#if defined(USE_MESH_ROUTER)
            if (RH_ROUTER_ERROR_NONE == send_data(data, total_datalen, to_node_id))
                return;
#else
            bool trial = false;
            if (router->getRouteTo(to_node_id)->state != RHRouter::Valid) {
                router->addRouteTo(to_node_id, to_node_id);
                trial = true;
            }
            if (RH_ROUTER_ERROR_NONE == send_data(data, total_datalen, to_node_id)) {
                return;
            } else {
                if (trial) {
                    router->deleteRouteTo(to_node_id);
                }
            }
#endif
        }
    }

    // send to the collector if all other nodes fail
    uint8_t flags = 0;
    uint8_t collector = collection_record->node_id;
    if (has_more_data) {
        flags = 1;
        p(F("Sending data to collector with FlAG: This node HAS MORE DATA\n"));
    }
    p(F("Delivering to collector: %d\n"), collector);
    if (router->getRouteTo(collector)->state != RHRouter::Valid) {
        p(F("Adding route to %d via %d\n"), collector, from);
        router->addRouteTo(collector, from);
    }
    send_data(data, total_datalen, collector, flags);
}

void _node_process_message(Message* msg, uint8_t len, uint8_t from)
{
    uint8_t datalen = len - sizeof(Message);
    p(F("Node process message FROM: %d LEN: %d\n"), from, datalen);
    if (datalen < 3) return;
    RecordSet* record_set = (RecordSet*)msg->data;
    p(F("Record set NODE_ID: %d; MESSAGE_ID: %d; RECORD_COUNT: %d\n"),
        record_set->node_id, record_set->message_id, record_set->record_count);
    for (int i=0; i<record_set->record_count; i++) {
        DataRecord record = record_set->records[i];
        switch (record.type) {
            case DATA_TYPE_NODE_COLLECTION_LIST :
            {
                p(F("Collection List. NODES: [ "));
                COLLECTION_LIST_STRUCT list = record.collection_list;
                uint8_t next_nodes_index;
                uint8_t prev_record_id;
                for (int n=0; n<list.node_count; n++) {
                    output(F("%d (prev record id: %d); "), list.nodes[n].node_id,
                        list.nodes[n].prev_record_id);
                    if (list.nodes[n].node_id == config.node_id) {
                        list.node_count--;
                        if (historical_data_shift_offset) {
                            prev_record_id = list.nodes[n].prev_record_id - historical_data_shift_offset;
                            historical_data_shift_offset = 0;
                        } else {
                            prev_record_id = list.nodes[n].prev_record_id;
                        }
                    } else {
                        list.nodes[next_nodes_index++] = {
                            .node_id = list.nodes[n].node_id,
                            .prev_record_id = list.nodes[n].prev_record_id
                        };
                    }
                }
                output(F("]\n"));
                p(F("Next nodes: [ "));
                for (int n=0; n<list.node_count; n++) {
                    output(F("%d (prev id: %d); "), list.nodes[n].node_id,
                        list.nodes[n].prev_record_id);
                }
                output(F("]\n"));
                break;
            }
            case DATA_TYPE_NEXT_ACTIVITY_SECONDS :
            {
                p(F("next activity\n"));
                break;
            }
            case DATA_TYPE_BATTERY_LEVEL :
            {
                p(F("battery level\n"));
                break;
            }
            case DATA_TYPE_SHARP_GP2Y1010AU0F :
            {
                p(F("sharp dust\n"));
                break;
            }
            case DATA_TYPE_WARN_50_PCT_DATA_HISTORY :
            {
                p(F("warn 50 pct\n"));
                break;
            }
        }
    }
/*
    uint8_t datalen = len - sizeof(Message);
    p(F("Node process message FROM: %d LEN: %d\n"), from, datalen);
    if (datalen < 3) return;
    uint8_t* data = msg->data;
    uint8_t new_data[MAX_MESSAGE_SIZE - sizeof(Message)];
    uint8_t index = 0;
    uint8_t new_data_index = 0;
    uint8_t next_nodes[MAX_NODES] = {0};
    uint8_t next_nodes_index = 0;
    uint8_t collector;
    uint8_t from_node_id = data[index++];
    new_data[new_data_index++] = from_node_id;
    uint8_t message_id = data[index++];
    new_data[new_data_index++] = message_id;
    uint8_t record_count = data[index++];
    new_data[new_data_index++] = record_count;
    uint8_t data_type = data[index++];
    //new_data[new_data_index++] = data_type;
    static uint8_t previous_max_record_id = 0;
    switch (data_type) {
        case DATA_TYPE_NODE_COLLECTION_LIST :
        {
            index--; // TODO: remove
            p(F("COLLECTION_LIST: "));
            collector = from_node_id;
            COLLECTION_LIST_STRUCT* data_struct =
                (COLLECTION_LIST_STRUCT*)&data[index];
            p(F("COLLECTION LIST STRUCT: type: %d; node_count: %d\n"),
                data_struct->type, data_struct->node_count);
            uint8_t new_nodes_index = 0;
            uint8_t node_count = data_struct->node_count;
            data_struct->node_count = 0;
            for (int i=0; i<node_count; i++) {
                CollectNodeStruct n = data_struct->nodes[i];
                output(F("[%d %d]"), n.node_id, n.previous_max_record_id);
                if (n.node_id == config.node_id) {
                    if (historical_data_shift_offset) {
                        previous_max_record_id = n.previous_max_record_id - historical_data_shift_offset;
                        historical_data_shift_offset = 0;
                    } else {
                        previous_max_record_id = n.previous_max_record_id;
                    }
                } else {
                    data_struct->nodes[new_nodes_index++] = {
                        .node_id = n.node_id,
                        .previous_max_record_id = n.previous_max_record_id
                    };
                    data_struct->node_count++;
                    next_nodes[next_nodes_index++] = n.node_id;
                }
            }
            //uint8_t len = sizeof(COLLECTION_LIST_STRUCT) +
            uint8_t len = 2 +
                data_struct->node_count * sizeof(CollectNodeStruct);
            p(F("memcpy data_struct len: %d into new_data at index: %d\n"),
                len, new_data_index);
            memcpy(&new_data[new_data_index], data_struct, len);
            new_data_index += len;
            index += len;
/*
            uint8_t node_count_index = new_data_index++;
            new_data[node_count_index] = 0;
            uint8_t node_count = data[index++];
            for (int i=0; i<node_count; i++) {
                uint8_t node = data[index++];
                uint8_t max_record = data[index++];
                output(F("NODE_ID: %d; MAX_RECORD_ID: %d; "),
                node, max_record);
                if (node == config.node_id) {
                    if (historical_data_shift_offset) {
                        previous_max_record_id = max_record - historical_data_shift_offset;
                        historical_data_shift_offset = 0;
                    } else {
                        previous_max_record_id = max_record;
                    }
                } else {
                    new_data[new_data_index++] = node;
                    new_data[new_data_index++] = max_record;
                    new_data[node_count_index]++;
                    next_nodes[next_nodes_index++] = node;
                }
            }
            output(F("\n"));
            break;
        }
        case DATA_TYPE_NEXT_ACTIVITY_SECONDS :
        {
            index--; // TODO: remove
            NEXT_ACTIVITY_SECONDS_STRUCT* data_struct =
                (NEXT_ACTIVITY_SECONDS_STRUCT*)&data[index];
            index += sizeof(NEXT_ACTIVITY_SECONDS_STRUCT);
            p(F("NEXT_ACTIVITY_SECONDS: %d\n"), data_struct->value);
            radio->sleep();
            next_activity_time = millis() + data_struct->value * 1000;
            return;
            break;
        }
    }
    // copy in the remaining data 
    while(index < datalen) {
        new_data[new_data_index++] = data[index++];
    }
    // After this point, we need to be sure not to overrun MAX_DATA_LENGTH 

    // add self data to new data 

    bool has_more_data = true;
    uint8_t max_record_id_index = 0;
    uint8_t added_record_count_index;
    if (new_data_index < MAX_DATA_LENGTH - 3) {
        //static uint8_t recent_max_record_id = 0;
        new_data[new_data_index++] = config.node_id;
        //new_data[new_data_index++] = ++recent_max_record_id;
        max_record_id_index = new_data_index;
        new_data[new_data_index++] = 0;
        added_record_count_index = new_data_index;
        new_data[new_data_index++] = 0; // record count

        // battery 
        if (new_data_index < MAX_DATA_LENGTH - 2) {
            BATTERY_LEVEL_STRUCT* data_struct =
                (BATTERY_LEVEL_STRUCT*)&new_data[new_data_index];
            *data_struct = {
                .type = DATA_TYPE_BATTERY_LEVEL,
                .value = (uint8_t)(roundf(batteryLevel() * 10))
            };
            new_data[added_record_count_index]++;
            new_data_index += sizeof(BATTERY_LEVEL_STRUCT);
        }

        // 50% data history warning 
        if ( (historical_data_index - historical_data_head) > HISTORICAL_DATA_SIZE / 2 ) {
            if (new_data_index < MAX_DATA_LENGTH - 1) {
                WARN_50_PCT_DATA_HISTORY_STRUCT* data_struct =
                    (WARN_50_PCT_DATA_HISTORY_STRUCT*)&new_data[new_data_index];
                *data_struct = {
                    .type = DATA_TYPE_WARN_50_PCT_DATA_HISTORY,
                };
                new_data[added_record_count_index]++;
                new_data_index += sizeof(WARN_50_PCT_DATA_HISTORY_STRUCT);
            }
        }

        //  add in historical data
        if (historical_data_index == 0) {
            has_more_data = false;
        }
        if (previous_max_record_id >= historical_data_index
                || previous_max_record_id == 255) {
            historical_data_head = 0; // buffer has filled and been reset
        } else {
            historical_data_head = previous_max_record_id + 1;
        }
        p(F("Traversing historical data from index %d to %d\n"),
                historical_data_head, historical_data_index);

//        for (int i=historical_data_head; i<historical_data_index
//        //for (int i=previous_max_record_id+1; i<historical_data_index
//                    && new_data_index < MAX_DATA_LENGTH - 7; i++) {
//            p(F("Setting data record from historical index: %d\n"), i);
//            SHARP_GP2Y1010AU0F_STRUCT* data_struct =
//                (SHARP_GP2Y1010AU0F_STRUCT*)&new_data[new_data_index];
//            *data_struct = {
//                .type = historical_data[i].type,
//                .value = historical_data[i].value,
//                .timestamp = historical_data[i].timestamp
//            };
//            new_data[added_record_count_index]++;
//            new_data[max_record_id_index] = i;
//            new_data_index += sizeof(SHARP_GP2Y1010AU0F_STRUCT);
//            if (i == historical_data_index - 1) {
//                p(F("No more historical data\n"));
//                has_more_data = false;
//            }
//        }

//
    }

    if (has_more_data) {
        p(F(" -------- There is still more historical data\n"));
    } else {
        p(F(" -------- There is NO more historical data\n"));
    }

    p(F("New data: [ "));
    for (int i=0; i<new_data_index; i++) output(F("%d "), new_data[i]);
    output(F("]\n"));

    if (!has_more_data && new_data_index < MAX_DATA_LENGTH - 10) {
        // only forward if we can fit at least a record
        //uint8_t ordered_nodes[MAX_NODES];
        //get_preferred_routing_order(next_nodes, next_nodes_index, ordered_nodes);
        for (int i=0; i<next_nodes_index; i++) {
            //uint8_t to_node_id = ordered_nodes[i];
            uint8_t to_node_id = next_nodes[i];
            p(F("Attempting forward to: %d\n"), to_node_id);
#if defined(USE_MESH_ROUTER)
            if (RH_ROUTER_ERROR_NONE == send_data(new_data, new_data_index, to_node_id))
                return;
#else
            bool trial = false;
            if (router->getRouteTo(to_node_id)->state != RHRouter::Valid) {
                router->addRouteTo(to_node_id, to_node_id);
                trial = true;
            }
            if (RH_ROUTER_ERROR_NONE == send_data(new_data, new_data_index, to_node_id)) {
                return;
            } else {
                if (trial) {
                    router->deleteRouteTo(to_node_id);
                }
            }
#endif
        }
    }

    // send to the collector if all other nodes fail 
    if (collector) {
        uint8_t flags = 0;
        if (has_more_data) {
            flags = 1;
            p(F("Sending data to collector with FlAG: This node HAS MORE DATA\n"));
        }
        p(F("Delivering to collector: %d\n"), collector);
        if (router->getRouteTo(collector)->state != RHRouter::Valid) {
            p(F("Adding route to %d via %d\n"), collector, from);
            router->addRouteTo(collector, from);
        }
        send_data(new_data, new_data_index, collector, flags);
    } else {
        p("WARNING: Did not forward and no collector!\n");
    }
    //router->printRoutingTable();

*/
}

static uint8_t uncollected_nodes[MAX_NODES];
static uint8_t uncollected_nodes_index = 0;

#define JSON_BUFFER_SIZE 500 // TODO: should be able to increase this to ~5000
static char json_buffer[JSON_BUFFER_SIZE] = {0};

void collector_process_message(Message* message, uint8_t len, uint8_t from, uint8_t flags)
{
    uint8_t datalen = len - sizeof(Message);
    if (datalen < 3) return;
    uint8_t* data = message->data;
    uint8_t index = 0;
    p("** Incoming message:\n");
    print_records(message->data, datalen);
    p("***\n");
    NewRecordSet* record_set = (NewRecordSet*)data;
    if (record_set->data[0] == DATA_TYPE_NODE_COLLECTION_LIST) {
        p(F("Extracting records into collection buffer index: %d\n"), collection_buffer_index);
        collection_buffer_index += extract_records(
            &collection_buffer[collection_buffer_index], message->data, datalen);
        p(F("New collection buffer index: %d\n"), collection_buffer_index);
        _COLLECTION_LIST* list = (_COLLECTION_LIST*)record_set->data;
        if (list->node_count > 0) {
            uint8_t nodes[20];
            for (int i=0; i< list->node_count; i++) {
                nodes[i] = list->nodes[i].node_id;
            }
            p(F("\nSending data collection request from collector_process_message\n"));
            send_data_collection_request(nodes, list->node_count);
            return;
        }
    }
    send_next_activity_seconds(30);
    next_activity_time = millis() + 30000 + 2000;
    p(F("Set next activity time: %d\n"), next_activity_time);
    /* TODO: POST collection data to API */
    p(F("*** Records to be posted to API collector_process_message ***\n"));
    print_records(collection_buffer, collection_buffer_index);
    memset(json_buffer, 0, JSON_BUFFER_SIZE);
    serialize_records(json_buffer, JSON_BUFFER_SIZE, collection_buffer, collection_buffer_index);
    Serial.println(json_buffer);
    collection_buffer_index = 0;
} /* collector_process_message */

void process_message(Message* msg, uint8_t len, uint8_t from, uint8_t flags) {
    if (config.node_type == NODE_TYPE_ORDERED_COLLECTOR) {
        p(F("Collected data received FROM: %d; MESSAGE_LEN: %d; FLAGS: %d\n"), from, len, flags);
        p(F("------->>>>>>>\n"));
        collector_process_message(msg, len, from, flags);
        p(F("<<<<<<<-------\n"));
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
    //memset(recv_buf, 0, RECV_BUFFER_SIZE);
    recv_buffer_avail = true;
}

bool receive_message(uint8_t* buf, uint8_t* len=NULL, uint8_t* source=NULL,
        uint8_t* dest=NULL, uint8_t* id=NULL, uint8_t* flags=NULL)
{
    static uint8_t last_message[RECV_BUFFER_SIZE] = {0};
    static uint8_t last_message_len = 0;
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
        if (*len == last_message_len) {
            /* why are we getting repeated messages? */
            bool same_message = true;
            for (int i=0; i<*len; i++) {
                if (last_message[i] != recv_buf[i]) {
                    same_message = false;
                    i = *len;
                }
            }
            if (same_message) {
                output(F("*"));
                return false;
            }
        }
        output("\n");
        memcpy(last_message, recv_buf, *len);
        last_message_len = *len;
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

void check_message()
{
    static bool listening = false;
    static uint16_t count = 0;
    uint8_t from, dest, msg_id, len, flags;
    if (!++count) output(F("."));
    if (receive_message(recv_buf, &len, &from, &dest, &msg_id, &flags)) {
        Message *_msg = (Message*)recv_buf;
        p(F("Received message of length: %d\n"), len);
        process_message(_msg, len, from, flags);
    }
    release_recv_buffer();
} /* check_incoming_message */


void send_data_collection_request(uint8_t* nodes, uint8_t node_count)
{
    static uint8_t msg_id = 0;
    uint8_t len = MAX_MESSAGE_SIZE;
    uint8_t buffer[MAX_MESSAGE_SIZE];
    uint8_t buf_index = 0;
    create_collection_record(config.node_id, ++msg_id, nodes,
        node_count, buffer, &len);
    /* TODO: use a preferred routing order */
    for (int i=0; i<node_count; i++) {
        uint8_t node_id = nodes[i]; // until get_preferred is working
#if defined(USE_MESH_ROUTER)
        if (RH_ROUTER_ERROR_NONE == send_data(
                buffer, len, node_id)) {
                //record_set, sizeof_record_set(&record_set), node_id)) {
            next_activity_time = millis() + 20000; // timeout in case no response from network
            return;
        }
#else
        bool trial = false;
        if (router->getRouteTo(node_id)->state != RHRouter::Valid) {
            router->addRouteTo(node_id, node_id);
            trial = true;
        }
        if (RH_ROUTER_ERROR_NONE == send_data(
                buffer, len, node_id)) {
                //(uint8_t*)&record_set, sizeof_record_set(&record_set), node_id)) {
            next_activity_time = millis() + 20000; // timeout in case no response from network
            p(F("Set next_activity_time for timeout: %d\n"), next_activity_time);
            return;
        } else {
            if (trial) {
                p(F("Removing unsuccessful trial route to: %d\n"), node_id);
                router->deleteRouteTo(node_id);
            }
        }
    }
    // No successful collection requests sent. Wait for next attempt //
    send_next_activity_seconds(30);
    next_activity_time = millis() + 30000 + 2000;
    /* TODO: POST collection data to API */
    p(F("*** Records to be posted to API (send_data_collection_request) ***\n"));
    print_records(collection_buffer, collection_buffer_index);
    memset(json_buffer, 0, JSON_BUFFER_SIZE);
    serialize_records(json_buffer, JSON_BUFFER_SIZE, collection_buffer, collection_buffer_index);
    Serial.println(json_buffer);
    collection_buffer_index = 0;
#endif
} /* send_data_collection_request */

void send_next_activity_seconds(uint16_t seconds)
{
    static uint8_t msg_id = 0;
    uint8_t buffer[MAX_MESSAGE_SIZE];
    uint8_t len = sizeof(buffer);
    create_next_activity_record(config.node_id, ++msg_id, seconds,
        buffer, &len);
    p(F("Broadcasting next activity seconds: %d\n"), seconds);
    send_data(buffer, len, RH_BROADCAST_ADDRESS);
} /* send_next_activity_seconds */

/* sensor data sampling */

void sharp_dust_sample()
{
    //int32_t val = SHARP_GP2Y1010AU0F::read_average(100);
    int32_t val = SHARP_GP2Y1010AU0F::read();
    p(F("DUST VAL: %d\n"), val);
    if (historical_data_index >= HISTORICAL_DATA_SIZE) {
        if (historical_data_head == 0) {
            p(F("WARNING: Historical data buffer full. Use a faster collection rate."));
            return;
        }
        uint8_t len = historical_data_index - historical_data_head;
        p(F("Shifting historical data and resetting\n"));
        memcpy(historical_data, historical_data + historical_data_head,
            len * sizeof(Data));
        historical_data_shift_offset = historical_data_head;
        historical_data_head = 0;
        historical_data_index = len;
    }
    historical_data[historical_data_index++] = {
        .id = ++data_id,
        .node_id = config.node_id,
        .timestamp = rtc.now().unixtime(),
        .type = DATA_TYPE_SHARP_GP2Y1010AU0F,
        .value = (int16_t)(val)
    };
}

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
    Serial.begin(115200);
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
    /* RadioHead sometimes continues retrying transmissions even after a
       successful reliable delivery. This seems to persist even when switching
       to a continually listening mode, effectively flooding the local
       air space while trying to listen for new messages. As a result, don't
       do any retries in the router. Instead, we will pick up missed messages
       in the application layer. */
    //router->setRetries(0);
    router->clearRoutingTable();
    //router->addRouteTo(1, 1);
    //router->addRouteTo(2, 2);
    //router->addRouteTo(3, 3);
    //router->addRouteTo(4, 4);
#endif
    if (USE_SLOW_RELIABLE_MODE)
        radio->setModemConfig(RH_RF95::Bw125Cr48Sf4096);
    if (!router->init()) p(F("Router init failed\n"));
    p(F("FREQ: %.2f\n"), config.rf95_freq);
    if (!radio->setFrequency(config.rf95_freq)) {
        p(F("Radio frequency set failed\n\n"));
    }
    radio->setTxPower(config.tx_power, false);
    radio->setCADTimeout(CAD_TIMEOUT);
    /* TODO: what is the correct timeout? From the RH docs:
    [setTimeout] Sets the minimum retransmit timeout. If sendtoWait is waiting
    for an ack longer than this time (in milliseconds), it will retransmit the
    message. Defaults to 200ms. The timeout is measured from the end of
    transmission of the message. It must be at least longer than the the transmit
    time of the acknowledgement (preamble+6 octets) plus the latency/poll time of
    the receiver. */
    //router->setTimeout(TIMEOUT);
    /* Verify packing of data type structs.
       TODO: can we make these compile-time assertions? Note: __assert is not
       being called on the Feather M0. Is there a way we can get something
       to print out for these? */
    assert(sizeof(SHARP_GP2Y1010AU0F_STRUCT) == 6);
    p(F("SHARP_GP2Y1010AU0F_STRUCT size verified\n"));
    assert(sizeof(BATTERY_LEVEL_STRUCT) == 1);
    p(F("BATTERY_LEVEL_STRUCT size verified\n"));
    p("size of 50pct: %d\n", sizeof(WARN_50_PCT_DATA_HISTORY_STRUCT));
    //assert(sizeof(WARN_50_PCT_DATA_HISTORY_STRUCT) == 0);
    //p(F("WARN_50_PCT_DATA_HISTORY_STRUCT size verified\n"));
    assert(sizeof(NEXT_ACTIVITY_SECONDS_STRUCT) == 2);
    p(F("NEXT_ACTIVITY_SECONDS_STRUCT size verified\n"));
    assert(sizeof(CollectNodeStruct) == 2);
    p(F("CollectNodeStruct size verified\n"));
    assert(sizeof(COLLECTION_LIST_STRUCT) == 1 + sizeof(CollectNodeStruct)*MAX_NODES);
    p(F("COLLECTION_LIST_STRUCT size verified\n"));

    p(F("Setup complete\n"));
    delay(100);
}

void loop()
{
    static unsigned long last_display_update = 0;
    static unsigned long last_dust_sample = 0;
    if (config.node_type == NODE_TYPE_ORDERED_COLLECTOR
            || millis() > next_activity_time) {
        check_message();
    } else {
        if (millis() - last_dust_sample > config.SHARP_GP2Y1010AU0F_DUST_PERIOD * 250) {
            sharp_dust_sample();
            last_dust_sample = millis();
        }
    }
    if (config.has_oled && millis() - last_display_update > DISPLAY_UPDATE_PERIOD) {
        update_display();
        last_display_update = millis();
    }
    if (config.node_type == NODE_TYPE_ORDERED_COLLECTOR) {
        if (millis() > next_activity_time) {
            send_data_collection_request(known_nodes, sizeof(known_nodes));
        }
    }
}

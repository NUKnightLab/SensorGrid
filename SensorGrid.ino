#include "SensorGrid.h"
#include "config.h"
#include "display.h"
#include <RHMesh.h>
#include <RHRouter.h>
#include <RH_RF95.h>
#include <SPI.h>
#include <pt.h>

static RH_RF95 radio(RF95_CS, RF95_INT);
static RHMesh* router;
static uint8_t message_id = 0;
static unsigned long next_listen = 0;

RTC_PCF8523 rtc;
Adafruit_FeatherOLED display = Adafruit_FeatherOLED();
uint32_t display_clock_time = 0;
static uint32_t oled_activated_time = 0;
bool oled_is_on;

volatile int aButtonState = 0;
volatile int bButtonState = 0;
volatile int cButtonState = 0;
static bool shutdown_requested = false;

uint8_t DATA_SIZE = sizeof(Data);
uint8_t MAX_MESSAGE_PAYLOAD = sizeof(Message);
uint8_t MESSAGE_OVERHEAD = sizeof(Message) - MAX_DATA_RECORDS * DATA_SIZE;

uint8_t recv_buf[MAX_MESSAGE_SIZE] = {0};
static bool recv_buffer_avail = true;

static Data historical_data[100] = {0};
static uint8_t historical_data_index = 0;
static uint8_t data_id = 0;

/**
 * Track the latest broadcast control message received for each node
 * 
 * This is the application layer control ID, not the RadioHead message ID since
 * RadioHead does not let us explictly set the ID for sendToWait
 */
uint8_t received_broadcast_control_messages[MAX_NODES];

/* Collection state */
uint8_t collector_id = 0;
uint8_t known_nodes[MAX_NODES];
uint8_t uncollected_nodes[MAX_NODES];
uint8_t pending_nodes[MAX_NODES];
bool pending_nodes_waiting_broadcast = false;
static long next_collection_time = 0;
int16_t last_rssi[MAX_NODES];

/* **** UTILS **** */

char* get_message_type(int8_t num)
{
    switch (num) {
        case MESSAGE_TYPE_CONTROL:
            return "CONTROL";
        case MESSAGE_TYPE_DATA:
            return "DATA";
        default:
            return "UNKNOWN";
    }
}

void add_pending_node(uint8_t id)
{
    int i;
    for (i=0; i<MAX_NODES && pending_nodes[i] != 0; i++) {
        if (pending_nodes[i] == id) {
            return;
        }
    }
    pending_nodes[i] = id;
    pending_nodes_waiting_broadcast = true;
}

void remove_pending_node(uint8_t id) {
    int dest = 0;
    for (int i=0; i<MAX_NODES; i++) {
        if (pending_nodes[i] != id)
            pending_nodes[dest++] = pending_nodes[i];
    }
}

void add_known_node(uint8_t id)
{
    int i;
    for (i=0; i<MAX_NODES && known_nodes[i] != 0; i++) {
        if (known_nodes[i] == id) {
            return;
        }
    }
    known_nodes[i] = id;
}

void remove_known_node_id(uint8_t id) {
    int dest = 0;
    for (int i=0; i<MAX_NODES; i++) {
        if (known_nodes[i] != id)
            known_nodes[dest++] = known_nodes[i];
    }
}

void remove_uncollected_node_id(uint8_t id) {
    int dest = 0;
    for (int i=0; i<MAX_NODES; i++) {
        if (uncollected_nodes[i] != id)
            uncollected_nodes[dest++] = uncollected_nodes[i];
    }
}

bool is_pending_nodes()
{
    return pending_nodes[0] > 0;
}

void clear_pending_nodes() {
    memset(pending_nodes, 0, MAX_NODES);
}

/* END OF UTILS */

/* **** SEND FUNCTIONS **** */

uint8_t send_message(uint8_t* msg, uint8_t len, uint8_t toID)
{
    p(F("Sending message type: %s"), ((Message*)msg)->message_type);
    p(F("; length: %d\n"), len);
    unsigned long start = millis();
    uint8_t err = router->sendtoWait(msg, len, toID);
    if (millis() < next_listen) {
        Serial.println("Listen timeout not expired. Sleeping.");
        radio.sleep();
    }
    p(F("Time to send: %d\n"), millis() - start);
    if (err == RH_ROUTER_ERROR_NONE) {
        return err;
    } else if (err == RH_ROUTER_ERROR_INVALID_LENGTH) {
        p(F("ERROR sending message to Node ID: %d. INVALID LENGTH\n"), toID);
        return err;
    } else if (err == RH_ROUTER_ERROR_NO_ROUTE) {
        p(F("ERROR sending message to Node ID: %d. NO ROUTE\n"), toID);
        return err;
    } else if (err == RH_ROUTER_ERROR_TIMEOUT) {
        p(F("ERROR sending message to Node ID: %d. TIMEOUT\n"), toID);
        return err;
    } else if (err == RH_ROUTER_ERROR_NO_REPLY) {
        p(F("ERROR sending message to Node ID: %d. NO REPLY\n"), toID);
        return err;
    } else if (err == RH_ROUTER_ERROR_UNABLE_TO_DELIVER) {
        p(F("ERROR sending message to Node ID: %d. UNABLE TO DELIVER\n"), toID);
        return err;   
    } else {
        p(F("ERROR sending message to Node ID: %d. UNKOWN ERROR CODE: %d\n"), toID, err);
        return err;
    }
}

uint8_t send_control(Control *control, uint8_t dest)
{
    Message msg = {
        .sensorgrid_version = config.sensorgrid_version,
        .network_id = config.network_id,
        .from_node = config.node_id,
        .message_type = MESSAGE_TYPE_CONTROL,
        .len = 1
    };
    memcpy(&msg.control, control, sizeof(Control));
    uint8_t len = sizeof(Control) + MESSAGE_OVERHEAD;
    return send_message((uint8_t*)&msg, len, dest);
}

uint8_t send_data(Data *data, uint8_t array_size, uint8_t dest, uint8_t from_id)
{
    if (!from_id) from_id = config.node_id;
    Message msg = {
        .sensorgrid_version = config.sensorgrid_version,
        .network_id = config.network_id,
        .from_node = from_id,
        .message_type = MESSAGE_TYPE_DATA,
        .len = array_size,
    };
    memcpy(msg.data, data, sizeof(Data)*array_size);
    uint8_t len = sizeof(Data)*array_size + MESSAGE_OVERHEAD;
    return send_message((uint8_t*)&msg, len, dest);
}

bool send_data(Data *data, uint8_t array_size, uint8_t dest)
{
    return send_data(data, array_size, dest, 0);
}


/* END OF SEND  FUNCTIONS */

/* **** RECEIVE FUNCTIONS. THESE FUNCTIONS HAVE DIRECT ACCESS TO recv_buf **** */

/** 
 *  Other functions should use these functions for receive buffer manipulations
 *  and should call release_recv_buffer after done with any buffered data
 */

void lock_recv_buffer()
{
    recv_buffer_avail = false;
}

void release_recv_buffer()
{
    recv_buffer_avail = true;
}

/**
 * This should not need to be used
 */
static void clear_recv_buffer()
{
    memset(recv_buf, 0, MAX_MESSAGE_SIZE);
}

void validate_recv_buffer(uint8_t len)
{
    // some basic checks for sanity
    int8_t message_type = ((Message*)recv_buf)->message_type;
    switch(message_type) {
        case MESSAGE_TYPE_DATA:
            if (len != sizeof(Data)*((Message*)recv_buf)->len + MESSAGE_OVERHEAD) {
                p(F("WARNING: Received message of type DATA with incorrect size: %d\n"), len);
            }
            break;
        case MESSAGE_TYPE_CONTROL:
            if (len != sizeof(Control) + MESSAGE_OVERHEAD) {
                p(F("WARNING: Received message of type CONTROL with incorrect size: %d\n"), len);
            }
            break;
        default:
            p(F("WARNING: Received message of unknown type: %s\n"), get_message_type(message_type));
    }
}


int8_t _receive_message(uint8_t* len=NULL, uint16_t timeout=NULL, uint8_t* source=NULL, uint8_t* dest=NULL, uint8_t* id=NULL, uint8_t* flags=NULL)
{
    if (len == NULL) {
        uint8_t _len;
        len = &_len;
    }
    *len = MAX_MESSAGE_SIZE;
    if (!recv_buffer_avail) {
        p(F("WARNING: Could not initiate receive message. Receive buffer is locked.\n"));
        return MESSAGE_TYPE_NONE_BUFFER_LOCK;
    }
    Message* _msg;
    lock_recv_buffer(); // lock to be released by calling client
    if (timeout) {
        if (router->recvfromAckTimeout(recv_buf, len, timeout, source, dest, id, flags)) {
            _msg = (Message*)recv_buf;
             if ( _msg->sensorgrid_version != config.sensorgrid_version ) {
                p(F("WARNING: Received message with wrong firmware version: %d\n"), _msg->sensorgrid_version);
                return MESSAGE_TYPE_WRONG_VERSION;
            }           
            if ( _msg->network_id != config.network_id ) {
                p(F("WARNING: Received message from wrong network: %d\n"), _msg->network_id);
                return MESSAGE_TYPE_WRONG_NETWORK;
            }
            validate_recv_buffer(*len);
            p(F("Received buffered message. len: %d; type: %s"), *len, get_message_type(_msg->message_type));
            p(F("; from: %d; rssi: %d\n"), *source, radio.lastRssi());
            last_rssi[*source] = radio.lastRssi();
            return _msg->message_type;
        } else {
            return MESSAGE_TYPE_NO_MESSAGE;
        }
    } else {
        if (router->recvfromAck(recv_buf, len, source, dest, id, flags)) {
            _msg = (Message*)recv_buf;
            if ( _msg->sensorgrid_version != config.sensorgrid_version ) {
                p(F("IGNORING: Received message with wrong firmware version: %d\n"), _msg->sensorgrid_version);
                return MESSAGE_TYPE_WRONG_VERSION;
            }
            if ( _msg->network_id != config.network_id ) {
                p(F("IGNORING: Received message from wrong network: %d\n"), _msg->network_id);
                return MESSAGE_TYPE_WRONG_NETWORK;
            }
            validate_recv_buffer(*len);
            p(F("Received buffered message. len: %d; type: %s"), *len, get_message_type(_msg->message_type));
            p(F("; from: %d; rssi: %d\n"), *source, radio.lastRssi());
            last_rssi[*source] = radio.lastRssi();
            return _msg->message_type;
        } else {
            return MESSAGE_TYPE_NO_MESSAGE;
        }
    }
}

int8_t receive(uint8_t* source=NULL, uint8_t* dest=NULL, uint8_t* id=NULL,
        uint8_t* len=NULL, uint8_t* flags=NULL)
{
    return _receive_message(len, NULL, source, dest, id, flags);
}

int8_t receive(uint16_t timeout, uint8_t* source=NULL, uint8_t* dest=NULL, uint8_t* id=NULL,
        uint8_t* len=NULL, uint8_t* flags=NULL)
{
    return _receive_message(len, timeout, source, dest, id, flags);
}

/**
 * Get the control array from the receive buffer and copy it's length to len
 */
Control get_control_from_buffer()
{
    if (recv_buffer_avail) {
        Serial.println("WARNING: Attempt to extract control from unlocked buffer");
    }
    Message* _msg = (Message*)recv_buf;
    if ( _msg->message_type != MESSAGE_TYPE_CONTROL) {
        p(F("WARNING: Attempt to extract control from non-control type: %s\n"), get_message_type(_msg->message_type));
    } 
    return _msg->control;
}

/**
 * Get the data array from the receive buffer and copy it's length to len
 */
Data* get_data_from_buffer(uint8_t* len)
{
    if (recv_buffer_avail) {
        Serial.println("WARNING: Attempt to extract data from unlocked buffer");
    }
    Message* _msg = (Message*)recv_buf;
    if (_msg->message_type != MESSAGE_TYPE_DATA) {
        p(F("WARNING: Attempt to extract data from non-data type: %s\n"), get_message_type(_msg->message_type));
    }
    *len = _msg->len;
    return _msg->data;
}

void check_collection_state() {
    if (!collector_id) add_pending_node(config.node_id);
    static long next_add_nodes_broadcast = 0;
    if(pending_nodes[0] > 0 && millis() > next_add_nodes_broadcast) {
        Serial.println("Pending nodes are waiting broadcast");
        Control control = { .id = ++message_id,
          .code = CONTROL_ADD_NODE, .from_node = config.node_id, .data = 0 }; //, .nodes = pending_nodes };
        memcpy(control.nodes, pending_nodes, MAX_NODES);
        if (RH_ROUTER_ERROR_NONE == send_control(&control, RH_BROADCAST_ADDRESS)) {
            Serial.println("-- Sent ADD_NODE control");
        } else {
            Serial.println("ERROR: did not successfully broadcast ADD NODE control");
        }
        next_add_nodes_broadcast = millis() + 20000;
    }
} /* check_collection_state */


void _rebroadcast_control_message(Message* _msg, uint8_t len, uint8_t dest)
{
    /* rebroadcast control messages to 255 */
    if (dest == RH_BROADCAST_ADDRESS) {   
        if (config.node_type != NODE_TYPE_ORDERED_COLLECTOR
                && _msg->control.from_node != config.node_id
                && received_broadcast_control_messages[_msg->control.from_node] != _msg->control.id) {
            received_broadcast_control_messages[_msg->control.from_node] = _msg->control.id;
            p(F("Rebroadcasting broadcast control message originally from ID: %d\n"), _msg->control.from_node);
            if (RH_ROUTER_ERROR_NONE == send_message(recv_buf, len, RH_BROADCAST_ADDRESS)) {
                Serial.println("-- Sent broadcast control");
            } else {
                Serial.println("ERROR: could not re-broadcast control");
            }
        } else if (received_broadcast_control_messages[_msg->control.from_node] == _msg->control.id) {
            Serial.println("NOT rebroadcasting control message recently received");
        }
    }
}

void _handle_control_add_node(Control _control)
{
    if (config.node_type == NODE_TYPE_ORDERED_COLLECTOR) {
        Serial.print("Received control code: ADD_NODES. Adding known IDs: ");
        for (int i=0; i<MAX_NODES && _control.nodes[i] != 0; i++) {
            Serial.print(_control.nodes[i]);
            Serial.print(" ");
            add_known_node(_control.nodes[i]);
        }
        Serial.println("");
    } else {
        Serial.print("Received control code: ADD_NODES. Adding pending IDs: ");
        for (int i=0; i<MAX_NODES && _control.nodes[i] != 0; i++) {
            Serial.print(_control.nodes[i]);
            Serial.print(" ");
            add_pending_node(_control.nodes[i]);
        }
        Serial.println("");
    }
}

void _handle_control_next_activity_time(Control _control, unsigned long receive_time)
{
    if (config.node_type != NODE_TYPE_ORDERED_COLLECTOR) {
        radio.sleep();
        bool self_in_list = false;
        for (int i=0; i<MAX_NODES; i++) {
            if (_control.nodes[i] == config.node_id) {
                self_in_list = true;
            }
        }
        p(F("Self in control list: %d\n"), self_in_list);
        if (self_in_list) {
            if (collector_id) {
                if (collector_id == _control.from_node) {
                    // nothing changes
                } else {
                    // This node has a collector. Do we switch collectors here if not the same?
                }
            }
        } else {
            if (!collector_id || collector_id == _control.from_node) {
                add_pending_node(config.node_id);
            }
        }
        p(F("Received control code: NEXT_ACTIVITY_TIME. Sleeping for: %d\n"), _control.data - (millis() - receive_time));
        next_listen = receive_time + _control.data;
    }
}

void _handle_control_message(Message* _msg, uint8_t len, uint8_t from, uint8_t dest, unsigned long receive_time)
{
    if (_msg->control.from_node == config.node_id) return; // ignore controls originating from self
    _rebroadcast_control_message(_msg, len, dest);
    Control _control = get_control_from_buffer();
    p(F("Received control message from: %d; Message ID: %d\n"), from, _control.id);
    if (_control.code == CONTROL_NONE) {
      Serial.println("Received control code: NONE. Doing nothing");
    } else if (_control.code == CONTROL_ADD_NODE) {
        _handle_control_add_node(_control);
    } else if (_control.code == CONTROL_NEXT_ACTIVITY_TIME) {
        _handle_control_next_activity_time(_control, receive_time);
    } else {
        p(F("WARNING: Received unexpected control code: %d\n"), _control.code);
    }
}

bool set_node_data(Data* data, uint8_t* record_count) {
    /* TODO: a node could have multiple data records to set. Set all within constraints of available
     *  uncollected data records and return false (or flag?) if we still have records left to set. Also
     *  set the record_count if new records are added (up to MAX_DATA_RECORDS)
     *  
     *  TODO: also add missing known nodes -- but do this only once and return directly to the collector
     *  otherwise we may thrash and saturate w/ extra entries due to data space being smaller than address space
     */
    int index;
    for (index=0; index<*record_count; index++) {
        if (data[index].node_id == config.node_id) {
            data[index] = {
                .id = ++data_id, .node_id = config.node_id, .timestamp = 0, .type = DATA_TYPE_BATTERY_LEVEL,
                .value = (int16_t)(roundf(batteryLevel() * 100))
            };
            break;
        } 
    }
    /* TODO: Don't overrun the record max */
    memcpy(&data[*record_count], historical_data, historical_data_index*sizeof(Data));
    memset(historical_data, 0, sizeof(Data)*100);
    *record_count += historical_data_index;
    historical_data_index = 0;
    return true;
}

void _collector_handle_data_message()
{
    uint8_t record_count;
    Data* data = get_data_from_buffer(&record_count);
    uint8_t missing_data_nodes[MAX_DATA_RECORDS] = {0};
    Serial.print("Collector received data from nodes:");
    for (int i=0; i<record_count; i++) {
        if (data[i].id > 0) {
            Serial.print(" ");
            Serial.print(data[i].node_id, DEC);
            // TODO: should be a check for node having more data
            remove_uncollected_node_id(data[i].node_id);
        }
    }
    Serial.println("");
    if (missing_data_nodes[0] > 0) {
        Serial.print("Nodes with missing data: ");
        for (int i=0; i<record_count && data[i].node_id>0; i++) {
            Serial.print(" ");
            Serial.print(missing_data_nodes[i], DEC);
        }
    }
    if (uncollected_nodes[0] > 0) {
        next_collection_time = 0;
    } else {
        int COLLECTION_DELAY = 2000;
        int16_t COLLECTION_PERIOD = 30000;
        send_control_next_activity_time(COLLECTION_PERIOD);
        next_collection_time = millis() + COLLECTION_PERIOD + COLLECTION_DELAY;
    }
    Serial.print("Data received: {");
    for (int i=0; i<record_count; i++) {
        Serial.print(" id: ");
        Serial.print(data[i].node_id, DEC);
        Serial.print(", value: ");
        Serial.print(data[i].value, DEC);
        Serial.print(";");
    }
    Serial.println(" }");
    /* TODO: post the data to the API and determine if there are more nodes to collect */
}


uint8_t get_best_next_node(Data* data, uint8_t num_data_records)
{
  uint8_t dest = 0;
    for (int i=0; i<num_data_records; i++) {
        RHRouter::RoutingTableEntry* route = router->getRouteTo(data[i].node_id);
        if (route->state == 2) { // what is RH constant name for a valid route?
            if (route->next_hop == data[i].node_id) {
                dest = data[i].node_id;
                p(F("Next node is single hop to ID: %d\n"), dest);
                break;
            } else {
                if (!dest) {
                    dest = data[i].node_id;
                    p(F("Potential next node is multihop to: %d\n"), dest);
                }
            }
        }
    }
    if (!dest && num_data_records > 0) {
        dest = data[0].node_id;
        p(F("No known routes found to remaining nodes. Sending to first node ID: %d\n"), dest);
    }
    return dest;
}

void get_preferred_routing_order(Data* data, uint8_t num_data_records, uint8_t* order)
{
    uint8_t first_pref[MAX_DATA_RECORDS] = {0};
    uint8_t first_pref_index = 0;
    uint8_t second_pref[MAX_DATA_RECORDS] = {0};
    uint8_t second_pref_index = 0;
    uint8_t third_pref[MAX_DATA_RECORDS] = {0};
    uint8_t third_pref_index = 0;
    for (int i=0; i<num_data_records; i++) {
        Serial.print("Checking Node ID: "); Serial.println(data[i].node_id, DEC);
        if (data[i].id == config.node_id) {
            Serial.println("Skipping self ID for preferred routing");
        } else if (data[i].id > 0) { // TODO: do this based on data type rather than ID
            p(F("Not routing to already collected node ID: %d\n"), data[i].node_id);
        } else {
            RHRouter::RoutingTableEntry* route = router->getRouteTo(data[i].node_id);
            if (route->state == 2) { // what is RH constant name for a valid route?
                if (route->next_hop == data[i].node_id) {
                    first_pref[first_pref_index++] = data[i].node_id;
                    p(F("Node is single hop to ID: %d\n"), data[i].node_id);
                } else {
                    second_pref[second_pref_index++] = data[i].node_id;
                    p(F("Node is multihop to: %d\n"), data[i].node_id);
                }
            } else {
                third_pref[third_pref_index++] = data[i].node_id;
                p(F("No known route to ID: %d\n"), data[i].node_id);
            }
        }
    }
    Serial.print("First pref:");
    for (int i=0; i<MAX_DATA_RECORDS && first_pref[i] > 0; i++) {
        p(F(" %d"), first_pref[i]);
    }
    Serial.println("");
    Serial.print("Second pref:");
    for (int i=0; i<MAX_DATA_RECORDS && second_pref[i] > 0; i++) {
        p(F(" %d"), second_pref[i]);
    }
    Serial.println("");
    Serial.print("Third pref:");
    for (int i=0; i<MAX_DATA_RECORDS && third_pref[i] > 0; i++) {
        p(F(" %d"), third_pref[i]);
    }
    Serial.println("");
    
    memcpy(first_pref+first_pref_index, second_pref, second_pref_index);
    memcpy(first_pref+first_pref_index+second_pref_index, third_pref, third_pref_index);
    Serial.print("Determined preferred routing: [");
    for (int i=0; i<MAX_DATA_RECORDS && first_pref[i] > 0; i++) {
        p(F(" %d"), first_pref[i]);
    }
    Serial.println(" ]");
    memcpy(order, first_pref, MAX_DATA_RECORDS);
}

void _node_handle_data_message()
{
    uint8_t from_id = ((Message*)recv_buf)->from_node;
    collector_id = from_id;
    uint8_t record_count;
    Data* data = get_data_from_buffer(&record_count);
    p(F("Received data array of length: %d from ID: %d containing data: {"), record_count, ((Message*)recv_buf)->from_node);
    for (int i=0; i<record_count; i++) {
        p(F(" id: %d; value: %d;"), data[i].node_id, data[i].value);
        remove_pending_node(data[i].node_id);
    }
    Serial.println("} ");
    if (pending_nodes[0]) {
        Serial.print("Known pending nodes: ");
        for (int i=0; i<MAX_NODES && pending_nodes[i]>0; i++) {
            p(F(" %d"), pending_nodes[i]);
            if (record_count < MAX_DATA_RECORDS) {
                data[record_count++] = {
                    .id = 0, .node_id = pending_nodes[i], .timestamp = 0, .type = 0, .value = 0
                };
            }
        } // TODO: How to handle pending nodes if we have a full record set?
        Serial.println("");
    }
    set_node_data(data, &record_count);
    /* TODO: set a flag in outgoing message to indicate if there are more records to collect from this node */
    bool success = false;
    uint8_t order[MAX_DATA_RECORDS] = {0};
    get_preferred_routing_order(data, record_count, order);
    Serial.print("SANITY CHECK on node routing order: ");
    for (int i=0; i<5; i++) {
        p(F(" %d"), order[i]);
    }
    Serial.println("");
    for (int idx=0; (idx<MAX_DATA_RECORDS) && (order[idx] > 0) && (!success); idx++) {
        if (RH_ROUTER_ERROR_NONE == send_data(data, record_count, order[idx], from_id)) {
            p(F("Forwarded data to node: %d\n"), order[idx]);
            success = true;
        } else {
            p(F("Failed to forward data to node: %d. Trying next node if available\n"), order[idx]);
        }
    }
    if (!success) { // send to the collector
        if (RH_ROUTER_ERROR_NONE == send_data(data, record_count, from_id, from_id)) {
            p(F("Forwarded data to collector node: %d\n"), from_id);
        }
    }
}

void check_incoming_message()
{
    uint8_t from;
    uint8_t dest;
    uint8_t msg_id;
    uint8_t len;
    int8_t msg_type = receive(&from, &dest, &msg_id, &len);
    unsigned long receive_time = millis();
    Message *_msg = (Message*)recv_buf;
    if (msg_type == MESSAGE_TYPE_NO_MESSAGE) {
        // Do nothing
    } else if (msg_type == MESSAGE_TYPE_CONTROL) {
        _handle_control_message(_msg, len, from, dest, receive_time);
    } else if (msg_type == MESSAGE_TYPE_DATA) {
        if (config.node_type == NODE_TYPE_ORDERED_COLLECTOR) {
            _collector_handle_data_message();
        } else {
            _node_handle_data_message();
        }
    } else if (msg_type == MESSAGE_TYPE_WRONG_VERSION) {
        // ignore
    } else if (msg_type == MESSAGE_TYPE_WRONG_NETWORK) {
        // ignore
    } else {
        p(F("WARNING: Received unexpected Message type: %s from ID: %d\n"), get_message_type(msg_type), from);
    }
    release_recv_buffer();
} /* check_incoming_message */

/* END OF RECEIVE FUNCTIONS */

/* **** TEST SPECIFIC FUNCTIONS **** */

bool send_aggregate_data_init() {

    if (uncollected_nodes[0] == 0) return false;
    
    Data data[MAX_DATA_RECORDS];
    uint8_t num_data_records = 0;
    for (int i=0; i<MAX_DATA_RECORDS && uncollected_nodes[i] != 0; i++) {
        data[i] = {
            .id = 0, .node_id = uncollected_nodes[i], .timestamp = 0, .type = 0, .value = 0 };
        Serial.print(uncollected_nodes[i], DEC);
        Serial.print(" ");
        num_data_records++;
    }
    uint8_t dest = get_best_next_node(data, num_data_records);
    if (!dest) {
        Serial.println("No remaining nodes in current data record");
    } if (RH_ROUTER_ERROR_NONE == send_data(data, num_data_records, dest)) {
        p(F("-- Sent data: AGGREGATE_DATA_INIT to ID: %d\n"), dest);
    } else {
        Serial.println("ERROR: did not successfully send aggregate data collection request");
        p(F("Removing node ID: %d from known_nodes\n"), dest);
        remove_known_node_id(dest);
        remove_uncollected_node_id(dest); // TODO: should there be some fallback or retry?
        p(F("** WARNING:: Node appears to be offline: %d\n"), dest);
        return send_aggregate_data_init();
    }
    return true;
} /* send_aggregate_data_init */

void send_control_next_activity_time(int16_t timeout)
{
    Control control = { .id = ++message_id,
          .code = CONTROL_NEXT_ACTIVITY_TIME, .from_node = config.node_id, .data = timeout };
    memcpy(control.nodes, known_nodes, MAX_NODES);
    Serial.println("Broadcasting next request time");
    if (RH_ROUTER_ERROR_NONE == send_control(&control, RH_BROADCAST_ADDRESS)) {
        Serial.print("-- Sent control: CONTROL_NEXT_ACTIVITY_TIME to nodes:");
        for (int i=0; i<MAX_NODES && control.nodes[i] > 0; i++) {
            p(F(" %d"), control.nodes[i]);
        }
        Serial.println("");
    } else {
        Serial.println("ERROR: did not successfully broadcast aggregate data collection request");
    }
} /* send_next_aggregate_data_request */

void handle_collector_loop()
{
    int16_t DATA_COLLECTION_TIMEOUT = 20000;
    bool collector_waiting_for_data = uncollected_nodes[0] > 0;
    static int cycle = 0;
    if (millis() > next_collection_time) {
            if (known_nodes[0] == 0) {
                Serial.println("No known nodes. Sending next activity signal for 10 sec");
                send_control_next_activity_time(10000);
                next_collection_time = millis() + 10000;
                return;
            }
            if (!collector_waiting_for_data) {
                memcpy(uncollected_nodes, known_nodes, MAX_NODES);
                Serial.print("Starting collection of known nodes: ");
                for (int i=0; i<MAX_NODES && known_nodes[i]>0; i++) {
                    p(F(" %d"), known_nodes[i]);
                }
                Serial.println("");
            }
            if (send_aggregate_data_init()) {
                p(F("Cycle: %d\n"), cycle++);
                next_collection_time = millis() + DATA_COLLECTION_TIMEOUT; // this is a timeout in case data does not come back from the network
            }
    }
}; /* test_aggregate_data_collection */

/* END OF TEST FUNCTIONS */


/* 
 * protothreads (https://github.com/fernandomalmeida/protothreads-arduino) 
 */

static struct pt update_clock_protothread;
static struct pt update_display_protothread;
static struct pt update_display_battery_protothread;
static struct pt display_timeout_protothread;
static struct pt sharp_dust_data_sample_protothread;

static int updateClockThread(struct pt *pt, int interval)
{
  static unsigned long timestamp = 0;
  PT_BEGIN(pt);
  while(1) { // never stop 
    PT_WAIT_UNTIL(pt, millis() - timestamp > interval );
    int gps_year = GPS.year;
    if (gps_year != 0 && gps_year != 80) {
        uint32_t gps_time = DateTime(GPS.year,GPS.month,GPS.day,GPS.hour,GPS.minute,GPS.seconds).unixtime();
        uint32_t rtc_time = rtc.now().unixtime();
        if (rtc_time - gps_time > 1 || gps_time - rtc_time > 1) { 
            rtc.adjust(DateTime(GPS.year,GPS.month,GPS.day,GPS.hour,GPS.minute,GPS.seconds));
        }
    }
    timestamp = millis();
  }
  PT_END(pt);
}

static int updateDisplayBatteryThread(struct pt *pt, int interval)
{
  static unsigned long timestamp = 0;
  PT_BEGIN(pt);
  while(1) { // never stop 
    PT_WAIT_UNTIL(pt, millis() - timestamp > interval );
    if (oled_is_on)
        updateDisplayBattery();
    timestamp = millis();
  }
  PT_END(pt);
}

static int updateDisplayThread(struct pt *pt, int interval)
{
  static unsigned long timestamp = 0;
  PT_BEGIN(pt);
  while(1) { // never stop 
    PT_WAIT_UNTIL(pt, millis() - timestamp > interval );
    if (oled_is_on)
        updateDisplay();
    timestamp = millis();
  }
  PT_END(pt);
}

static int displayTimeoutThread(struct pt *pt, int interval)
{
  static unsigned long timestamp = 0;
  PT_BEGIN(pt);
  while(1) { // never stop 
    PT_WAIT_UNTIL(pt, millis() - timestamp > interval );
    if ( config.display_timeout > 0 && (millis() - oled_activated_time) > config.display_timeout*1000) {
        oled_is_on = false;
        shutdown_requested = false;
        display.clearDisplay();
        display.display();
    }
    timestamp = millis();
  }
  PT_END(pt);
}


static int sharpDustDataSampleThread(struct pt *pt, int interval)
{
  static unsigned long timestamp = 0;
  PT_BEGIN(pt);
  while(1) { // never stop 
    PT_WAIT_UNTIL(pt, millis() - timestamp > interval );
    int32_t val = SHARP_GP2Y1010AU0F::read_average(100);
    p(F("DUST VAL: %d\n"), val);
    historical_data[historical_data_index++] = {
      .id = ++data_id, .node_id = config.node_id, .timestamp = 0, .type = DATA_TYPE_SHARP_GP2Y1010AU0F,
      .value = (int16_t)(val)
    };
    timestamp = millis();
  }
  PT_END(pt);
}


/*
 * interrupts
 */

void aButton_ISR()
{
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
    Serial.println("B Button");
    Serial.println(shutdown_requested);
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
    Serial.println("C Button");
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

/* **** SETUP and LOOP **** */

void check_radiohead_version()
{
    Serial.print("Setting up radio with RadioHead Version ");
    Serial.print(RH_VERSION_MAJOR, DEC); Serial.print(".");
    Serial.println(RH_VERSION_MINOR, DEC);
    /* TODO: Can RH version check be done at compile time? */
    if (RH_VERSION_MAJOR != REQUIRED_RH_VERSION_MAJOR 
        || RH_VERSION_MINOR != REQUIRED_RH_VERSION_MINOR) {
        Serial.print("ABORTING: SensorGrid requires RadioHead version ");
        Serial.print(REQUIRED_RH_VERSION_MAJOR, DEC); Serial.print(".");
        Serial.println(REQUIRED_RH_VERSION_MINOR, DEC);
        Serial.print("RadioHead ");
        Serial.print(RH_VERSION_MAJOR, DEC); Serial.print(".");
        Serial.print(RH_VERSION_MINOR, DEC);
        Serial.println(" is installed");
        while(1);
    }
}

void setup()
{
    //while (!Serial);
    check_radiohead_version();
    loadConfig();
    p(F("Node ID: %d\n"), config.node_id);

    Serial.print("Node type: "); Serial.println(config.node_type);
    pinMode(LED, OUTPUT);
    pinMode(config.RFM95_RST, OUTPUT);
    pinMode(BUTTON_A, INPUT_PULLUP);
    pinMode(BUTTON_B, INPUT_PULLUP);
    pinMode(BUTTON_C, INPUT_PULLUP);  
    digitalWrite(LED, LOW);
    digitalWrite(config.RFM95_RST, HIGH);
    
    if (config.has_oled) {
        Serial.print(F("Display timeout set to: ")); Serial.println(config.display_timeout);
        setupDisplay();
        oled_is_on = true;
        attachInterrupt(BUTTON_A, aButton_ISR, CHANGE);
        attachInterrupt(BUTTON_B, bButton_ISR, FALLING);
        attachInterrupt(BUTTON_C, cButton_ISR, FALLING);
    }
    
    rtc.begin();
    
    router = new RHMesh(radio, config.node_id);
    //rf95.setModemConfig(RH_RF95::Bw125Cr48Sf4096);
    if (!router->init())
        Serial.println("Router init failed");
    p(F("FREQ: %d\n"), config.rf95_freq);
    if (!radio.setFrequency(config.rf95_freq)) {
        Serial.println("Radio frequency set failed");
    } 
    radio.setTxPower(config.tx_power, false);
    radio.setCADTimeout(CAD_TIMEOUT);
    router->setTimeout(TIMEOUT);
    Serial.println("");
    delay(100);

    setupSensors();

    /* initialize protothreads */
    PT_INIT(&update_clock_protothread);
    if (config.has_oled) {
        PT_INIT(&update_display_protothread);
        PT_INIT(&update_display_battery_protothread);
        PT_INIT(&display_timeout_protothread);
    }
    PT_INIT(&sharp_dust_data_sample_protothread);
}

void loop()
{
    if (config.node_type == NODE_TYPE_ORDERED_COLLECTOR) {
        handle_collector_loop();
        check_incoming_message();
    } else {
        if (millis() >= next_listen) {
            check_collection_state();
            check_incoming_message();
        }
    }

    sharpDustDataSampleThread(&sharp_dust_data_sample_protothread, config.SHARP_GP2Y1010AU0F_DUST_PERIOD * 1000);
    if (config.has_oled) {
        updateDisplayThread(&update_display_protothread, 1000);
        updateDisplayBatteryThread(&update_display_battery_protothread, 10 * 1000);
        displayTimeoutThread(&display_timeout_protothread, 1000);
    }
}

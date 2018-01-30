#include <RHMesh.h>
#include <RHRouter.h>
#include <RH_RF95.h>
#include <SPI.h>

/* SET THIS FOR EACH NODE */
#define NODE_ID 1 // 1 is collector; 2,3 are sensors
#define COLLECTOR_NODE_ID 1

#define FREQ 915.00
#define TX 5
#define CAD_TIMEOUT 1000
#define TIMEOUT 1000
#define RF95_CS 8
#define REQUIRED_RH_VERSION_MAJOR 1
#define REQUIRED_RH_VERSION_MINOR 82
#define RF95_INT 3
#define MAX_MESSAGE_SIZE 255
#define NETWORK_ID 3
#define SENSORGRID_VERSION 1

/**
 * Overall max message size is somewhere between 244 and 247 bytes. 247 will cause invalid length error
 * 
 * Note that these max sizes on the message structs are system specific due to struct padding. The values
 * here are specific to the Cortex M0
 * 
 */
#define MAX_DATA_RECORDS 39
#define MAX_CONTROL_NODES 230

// test types
#define AGGREGATE_DATA_COLLECTION_WITH_SLEEP_TEST 4
#define TEST_TYPE AGGREGATE_DATA_COLLECTION_WITH_SLEEP_TEST

/* *
 *  Message types:
 *  Not using enum for message types to ensure small numeric size
 */
#define MESSAGE_TYPE_NO_MESSAGE 0
#define MESSAGE_TYPE_CONTROL 1 
#define MESSAGE_TYPE_DATA 2
#define MESSAGE_TYPE_UNKNOWN -1
#define MESSAGE_TYPE_MESSAGE_ERROR -2
#define MESSAGE_TYPE_NONE_BUFFER_LOCK -3
#define MESSAGE_TYPE_WRONG_VERSION -4
#define MESSAGE_TYPE_WRONG_NETWORK -5 // for testing only. Normally we will just skip messages from other networks

/**
 * Control codes
 */
#define CONTROL_SEND_DATA 1
#define CONTROL_NEXT_COLLECTION 2
#define CONTROL_NONE 3 // no-op used for testing
#define CONTROL_NEXT_REQUEST_TIME 5
#define CONTROL_ADD_NODE 6

/* Data types */
#define AGGREGATE_DATA_INIT 0

static RH_RF95 radio(RF95_CS, RF95_INT);
static RHMesh* router;
static uint8_t message_id = 0;
static unsigned long next_listen = 0;

/* Defining list of nodes */
int sensorArray[2] = {};

typedef struct Control {
    uint8_t id;
    uint8_t code;
    uint8_t from_node;
    int16_t data;
    uint8_t nodes[MAX_CONTROL_NODES];
};

typedef struct Data {
    uint8_t id; // 1-255 indicates Data
    uint8_t node_id;
    uint8_t timestamp;
    int8_t type;
    int16_t value;
};

typedef struct MultidataMessage {
    uint8_t sensorgrid_version;
    uint8_t network_id;
    uint8_t from_node;
    uint8_t message_type;
    uint8_t len;
    union {
      struct Control control;
      struct Data data[MAX_DATA_RECORDS];
    };
};

uint8_t MAX_MESSAGE_PAYLOAD = sizeof(Data);
uint8_t MAX_MULIDATA_MESSAGE_PAYLOAD = sizeof(MultidataMessage);
uint8_t MULTIDATA_MESSAGE_OVERHEAD = sizeof(MultidataMessage) - MAX_DATA_RECORDS * MAX_MESSAGE_PAYLOAD;

uint8_t recv_buf[MAX_MESSAGE_SIZE] = {0};
static bool recv_buffer_avail = true;

/**
 * Track the latest broadcast control message received for each node
 * 
 * This is the application layer control ID, not the RadioHead message ID since
 * RadioHead does not let us explictly set the ID for sendToWait
 */
uint8_t received_broadcast_control_messages[MAX_CONTROL_NODES];

/* Collection state */
uint8_t collector_id;
uint8_t known_nodes[MAX_CONTROL_NODES];
uint8_t pending_nodes[MAX_CONTROL_NODES];
bool pending_nodes_waiting_broadcast = false;

/* **** UTILS **** */

void print_message_type(int8_t num)
{
    switch (num) {
        case MESSAGE_TYPE_CONTROL:
            Serial.print("CONTROL");
            break;
        case MESSAGE_TYPE_DATA:
            Serial.print("DATA");
            break;
        default:
            Serial.print("UNKNOWN");
    }
}

unsigned long hash(uint8_t* msg, uint8_t len)
{
    unsigned long h = 5381;
    for (int i=0; i<len; i++){
        h = ((h << 5) + h) + msg[i];
    }
    return h;
}

unsigned checksum(void *buffer, size_t len, unsigned int seed)
{
      unsigned char *buf = (unsigned char *)buffer;
      size_t i;
      for (i = 0; i < len; ++i)
            seed += (unsigned int)(*buf++);
      return seed;
}

extern "C" char *sbrk(int i);
static int free_ram()
{
    char stack_dummy = 0;
    return &stack_dummy - sbrk(0);
}

void print_ram()
{
    Serial.print("Avail RAM: ");
    Serial.println(free_ram(), DEC);
}

void add_pending_node(uint8_t id)
{
    int i;
    for (i=0; i<MAX_CONTROL_NODES && pending_nodes[i] != 0; i++) {
        if (pending_nodes[i] == id) {
            return;
        }
    }
    pending_nodes[i] = id;
    pending_nodes_waiting_broadcast = true;
}

void add_known_node(uint8_t id)
{
    int i;
    for (i=0; i<MAX_CONTROL_NODES && known_nodes[i] != 0; i++) {
        if (known_nodes[i] == id) {
            return;
        }
    }
    known_nodes[i] = id;
}

void remove_known_node_id(uint8_t id) {
    int dest = 0;
    for (int i=0; i<MAX_CONTROL_NODES; i++) {
        if (known_nodes[i] != id)
            known_nodes[dest++] = known_nodes[i];
    }
}

bool is_pending_nodes()
{
    return pending_nodes[0] > 0;
}

void clear_pending_nodes() {
    memset(pending_nodes, 0, MAX_CONTROL_NODES);
}
/* END OF UTILS */

/* **** SEND FUNCTIONS **** */

bool send_message(uint8_t* msg, uint8_t len, uint8_t toID)
{
    Serial.print("Sending message type: ");
    print_message_type(((MultidataMessage*)msg)->message_type);
    Serial.print("; length: ");
    Serial.println(len, DEC);
    unsigned long start = millis();
    uint8_t err = router->sendtoWait(msg, len, toID);
    if (millis() < next_listen) {
        Serial.print("Listen timeout not expired. Sleeping.");
        radio.sleep();
    }
    Serial.print("Time to send: ");
    Serial.println(millis() - start);
    if (err == RH_ROUTER_ERROR_NONE) {
        return true;
    } else if (err == RH_ROUTER_ERROR_INVALID_LENGTH) {
        Serial.print(F("ERROR sending message to Node ID: "));
        Serial.print(toID);
        Serial.println(". INVALID LENGTH");
        return false;
    } else if (err == RH_ROUTER_ERROR_NO_ROUTE) {
        Serial.print(F("ERROR sending message to Node ID: "));
        Serial.print(toID);
        Serial.println(". NO ROUTE");
        return false;
    } else if (err == RH_ROUTER_ERROR_TIMEOUT) {
        Serial.print(F("ERROR sending message to Node ID: "));
        Serial.print(toID);
        Serial.println(". TIMEOUT");
        return false;
    } else if (err == RH_ROUTER_ERROR_NO_REPLY) {
        Serial.print(F("ERROR sending message to Node ID: "));
        Serial.print(toID);
        Serial.println(". NO REPLY");
        return false;
    } else if (err == RH_ROUTER_ERROR_UNABLE_TO_DELIVER) {
        Serial.print(F("ERROR sending message to Node ID: "));
        Serial.print(toID);
        Serial.println(". UNABLE TO DELIVER");
        return false;   
    } else {
        Serial.print(F("ERROR sending message to Node ID: "));
        Serial.print(toID);
        Serial.print(". UNKNOWN ERROR CODE: ");
        Serial.println(err, DEC);
        return false;
    }
}

bool send_multidata_control(Control *control, uint8_t dest)
{
    MultidataMessage msg = {
        .sensorgrid_version = SENSORGRID_VERSION,
        .network_id = NETWORK_ID,
        .from_node = NODE_ID,
        .message_type = MESSAGE_TYPE_CONTROL,
        .len = 1
    };
    memcpy(&msg.control, control, sizeof(Control));
    uint8_t len = sizeof(Control) + MULTIDATA_MESSAGE_OVERHEAD;
    return send_message((uint8_t*)&msg, len, dest);
}

bool send_multidata_data(Data *data, uint8_t array_size, uint8_t dest, uint8_t from_id)
{
    if (!from_id) from_id = NODE_ID;
    MultidataMessage msg = {
        .sensorgrid_version = SENSORGRID_VERSION,
        .network_id = NETWORK_ID,
        .from_node = from_id,
        .message_type = MESSAGE_TYPE_DATA,
        .len = array_size,
    };
    memcpy(msg.data, data, sizeof(Data)*array_size);
    uint8_t len = sizeof(Data)*array_size + MULTIDATA_MESSAGE_OVERHEAD;
    return send_message((uint8_t*)&msg, len, dest);
}

bool send_multidata_data(Data *data, uint8_t array_size, uint8_t dest)
{
    return send_multidata_data(data, array_size, dest, 0);
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
    int8_t message_type = ((MultidataMessage*)recv_buf)->message_type;
    switch(message_type) {
        case MESSAGE_TYPE_DATA:
            if (len != sizeof(Data)*((MultidataMessage*)recv_buf)->len + MULTIDATA_MESSAGE_OVERHEAD) {
                Serial.print("WARNING: Received message of type DATA with incorrect size: ");
                Serial.println(len, DEC);
            }
            break;
        case MESSAGE_TYPE_CONTROL:
            if (len != sizeof(Control) + MULTIDATA_MESSAGE_OVERHEAD) {
                Serial.print("WARNING: Received message of type CONTROL with incorrect size: ");
                Serial.println(len, DEC);
            }
            break;
        default:
            Serial.print("WARNING: Received message of unknown type: ");
            Serial.println(message_type);
    }
}


int8_t _receive_message(uint8_t* len=NULL, uint16_t timeout=NULL, uint8_t* source=NULL, uint8_t* dest=NULL, uint8_t* id=NULL, uint8_t* flags=NULL)
{
    //Serial.println("_receive_message");
    if (len == NULL) {
        uint8_t _len;
        len = &_len;
    }
    *len = MAX_MESSAGE_SIZE;
    if (!recv_buffer_avail) {
        Serial.println("WARNING: Could not initiate receive message. Receive buffer is locked.");
        return MESSAGE_TYPE_NONE_BUFFER_LOCK;
    }
    MultidataMessage* _msg;
    lock_recv_buffer(); // lock to be released by calling client
    if (timeout) {
        if (router->recvfromAckTimeout(recv_buf, len, timeout, source, dest, id, flags)) {
            _msg = (MultidataMessage*)recv_buf;
             if ( _msg->sensorgrid_version != SENSORGRID_VERSION ) {
                Serial.print("WARNING: Received message with wrong firmware version: ");
                Serial.println(_msg->sensorgrid_version, DEC);
                return MESSAGE_TYPE_WRONG_VERSION;
            }           
            if ( _msg->network_id != NETWORK_ID ) {
                Serial.print("WARNING: Received message from wrong network: ");
                Serial.println(_msg->network_id, DEC);
                return MESSAGE_TYPE_WRONG_NETWORK;
            }
            validate_recv_buffer(*len);
            Serial.print("Received buffered message. len: "); Serial.print(*len, DEC);
            Serial.print("; type: "); print_message_type(_msg->message_type);
            Serial.print("; from: "); Serial.println(*source);
            return _msg->message_type;
        } else {
            return MESSAGE_TYPE_NO_MESSAGE;
        }
    } else {
        if (router->recvfromAck(recv_buf, len, source, dest, id, flags)) {
            _msg = (MultidataMessage*)recv_buf;
            if ( _msg->sensorgrid_version != SENSORGRID_VERSION ) {
                Serial.print("WARNING: Received message with wrong firmware version: ");
                Serial.println(_msg->sensorgrid_version, DEC);
                return MESSAGE_TYPE_WRONG_VERSION;
            }
            if ( _msg->network_id != NETWORK_ID ) {
                Serial.print("WARNING: Received message from wrong network: ");
                Serial.println(_msg->network_id, DEC);
                return MESSAGE_TYPE_WRONG_NETWORK;
            }
            validate_recv_buffer(*len);
            Serial.print("Received buffered message. len: "); Serial.print(*len, DEC);
            Serial.print("; type: "); print_message_type(_msg->message_type);
            Serial.print("; from: "); Serial.println(*source);
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
Control get_multidata_control_from_buffer()
{
    if (recv_buffer_avail) {
        Serial.println("WARNING: Attempt to extract control from unlocked buffer");
    }
    MultidataMessage* _msg = (MultidataMessage*)recv_buf;
    if ( _msg->message_type != MESSAGE_TYPE_CONTROL) {
        Serial.print("WARNING: Attempt to extract control from non-control type: ");
        Serial.println(_msg->message_type, DEC);
    } 
    //*len = _msg->len;
    return _msg->control;
}

/**
 * Get the data array from the receive buffer and copy it's length to len
 */
Data* get_multidata_data_from_buffer(uint8_t* len)
{
    if (recv_buffer_avail) {
        Serial.println("WARNING: Attempt to extract data from unlocked buffer");
    }
    MultidataMessage* _msg = (MultidataMessage*)recv_buf;
    if (_msg->message_type != MESSAGE_TYPE_DATA) {
        Serial.print("WARNING: Attempt to extract data from non-data type: ");
        Serial.println(_msg->message_type, DEC);
    }
    *len = _msg->len;
    return _msg->data;
}

/*
void handle_incoming_aggregate_data()
{
     uint8_t from_id = ((MultidataMessage*)recv_buf)->from_node;
     uint8_t record_count;
     Data* data = get_multidata_data_from_buffer(&record_count);
     for (int i=0; i<record_count; i++) {
        if (data[i].id == 0) {
            if (data[i].node_id == NODE_ID) {
                Serial.println("Time for current node to collect");
                data[i] = {
                    .id = ++message_id, .node_id = NODE_ID, .timestamp = 0, .type = 1, .value = 12345
                };
                Serial.print("Sending aggregate data containing IDs: ");
                for (int i=0; i<record_count; i++) {
                    Serial.print(data[i].node_id);
                    Serial.print(", ");
                }
                uint8_t next_node_id;
                if (i == record_count - 1) {
                    next_node_id = from_id;
                } else {
                    next_node_id = data[i+1].node_id;
                }
                Serial.println("");
                if (send_multidata_data(data, record_count, next_node_id, from_id)) {
                    Serial.print("Forwarded data to node: ");
                    Serial.println(next_node_id, DEC);
                    Serial.println("");
                }
            } else {
                break; // there is still an uncollected node before current node
            }
        } else {
            // continue through to find the first 0 id  
        }
    }
}
*/

void check_collection_state() {
    if (pending_nodes_waiting_broadcast) {
        Serial.println("pending nodes are waiting broadcast");
        Control control = { .id = ++message_id,
          .code = CONTROL_ADD_NODE, .from_node = NODE_ID, .data = 0 }; //, .nodes = pending_nodes };
        memcpy(control.nodes, pending_nodes, MAX_CONTROL_NODES);
        if (send_multidata_control(&control, RH_BROADCAST_ADDRESS)) {
            Serial.println("-- Sent ADD_NODE control");
            pending_nodes_waiting_broadcast = false;
        } else {
            Serial.println("ERROR: did not successfully broadcast ADD NODE control");
        }
    }
} /* check_collection_state */

void _handle_control_message(MultidataMessage* _msg, uint8_t len, uint8_t from, uint8_t dest, unsigned long receive_time)
{
    if (_msg->control.from_node == NODE_ID) return;
    
    /* rebroadcast control messages to 255 */
    if (dest == RH_BROADCAST_ADDRESS) {   
        //if ( !(NODE_ID == COLLECTOR_NODE_ID && from == 3) ) { // TODO: ignoring 3 just for testing
        if ( !(NODE_ID == COLLECTOR_NODE_ID) ) {
            
            if (NODE_ID != COLLECTOR_NODE_ID // is there a reason for collectors to re-broadcast 255 controls? 
                    && _msg->control.from_node != NODE_ID
                    && received_broadcast_control_messages[_msg->control.from_node] != _msg->control.id) {
                received_broadcast_control_messages[_msg->control.from_node] = _msg->control.id;
                Serial.print("Rebroadcasting broadcast control message originally from ID: ");
                Serial.println(_msg->control.from_node, DEC);
                if (send_message(recv_buf, len, RH_BROADCAST_ADDRESS)) {
                    Serial.println("-- Sent broadcast control");
                } else {
                    Serial.println("ERROR: could not re-broadcast control");
                }
            //} else if (_msg->control.from_node == NODE_ID) {
            //    Serial.println("NOT rebroadcasting control message originating from self");
            } else if (received_broadcast_control_messages[_msg->control.from_node] == _msg->control.id) {
                Serial.println("NOT rebroadcasting control message recently received");
            }
        }
    }
    Control _control = get_multidata_control_from_buffer();
    Serial.print("Received control message from: "); Serial.print(from, DEC);
    Serial.print("; Message ID: "); Serial.println(_control.id, DEC);
    if (_control.code == CONTROL_NONE) {
      Serial.println("Received control code: NONE. Doing nothing");
    } else if (_control.code == CONTROL_SEND_DATA) {
        // TODO: check the dest ID. This should not be a broadcast message
       Data data[] = { {
          .id = ++message_id,
          .node_id = NODE_ID,
          .timestamp = 12345,
          .type = 111,
          .value = 123
        } };
        if (send_multidata_data(data, 1, from)) {
            Serial.println("Returned data");
            Serial.println("");
        }
    } else if (_control.code == CONTROL_ADD_NODE) {
    //else if (_control.code == CONTROL_ADD_NODE && !(NODE_ID == COLLECTOR_NODE_ID && from == 3 && dest == RH_BROADCAST_ADDRESS) ) { 
                                                    // TODO: ignoring broadcasts from 3 for testing
        if (NODE_ID == COLLECTOR_NODE_ID) {
            Serial.print("Received control code: ADD_NODES. Adding known IDs: ");
            for (int i=0; i<MAX_CONTROL_NODES && _control.nodes[i] != 0; i++) {
                Serial.print(_control.nodes[i]);
                Serial.print(", ");
                add_known_node(_control.nodes[i]);
            }
            Serial.println("");
        } else {
            Serial.print("Received control code: ADD_NODES. Adding pending IDs: ");
            for (int i=0; i<MAX_CONTROL_NODES && _control.nodes[i] != 0; i++) {
                Serial.print(_control.nodes[i]);
                Serial.print(", ");
                add_pending_node(_control.nodes[i]);
            }
            Serial.println("");
        }
    } else if (_control.code == CONTROL_NEXT_REQUEST_TIME) {
        if (NODE_ID != COLLECTOR_NODE_ID) {
            radio.sleep();
            bool self_in_list = false;
            for (int i=0; i<MAX_CONTROL_NODES; i++) {
                if (_control.nodes[i] == NODE_ID) {
                    self_in_list = true;
                }
            }
            Serial.print("Self in control list: ");
            Serial.println(self_in_list);
            if (self_in_list) {
                if (collector_id > 0) {
                    if (collector_id == _control.from_node) {
                        // nothing changes
                    } else {
                        // This node has a collector. Do we switch collectors here if not the same?
                    }
                }
            } else {
                if (collector_id <= 0 || collector_id == _control.from_node) {
                    add_pending_node(NODE_ID);
                    pending_nodes_waiting_broadcast = true; // broadcast self as pending even
                }                                            // if previous attempt
            }
            Serial.print("Received control code: NEXT_REQUEST_TIME. Sleeping for: ");
            Serial.println(_control.data - (millis() - receive_time));
            Serial.println("");
            next_listen = receive_time + _control.data;
        } 
    } else {
        Serial.print("WARNING: Received unexpected control code: ");
        Serial.println(_control.code);
    }
}

void _handle_data_message()
{
    //uint8_t len;
    uint8_t from_id = ((MultidataMessage*)recv_buf)->from_node;
    uint8_t record_count;
    Data* data = get_multidata_data_from_buffer(&record_count);
    Serial.print("Received data array of length: ");
    Serial.print(record_count, DEC);
    Serial.print(" from ID: ");
    Serial.print( ((MultidataMessage*)recv_buf)->from_node, DEC);
    Serial.print(" containing data: {");
    for (int i=0; i<record_count; i++) {
        Serial.print(" id: ");
        Serial.print(data[i].node_id, DEC);
        Serial.print(", value: ");
        Serial.print(data[i].value, DEC);
        Serial.print(";");
    }
    Serial.println("} ");
    //handle_incoming_aggregate_data();
     //uint8_t from_id = ((MultidataMessage*)recv_buf)->from_node;
     //uint8_t record_count;
     //Data* data = get_multidata_data_from_buffer(&record_count);
     for (int i=0; i<record_count; i++) {
        if (data[i].id == 0) {
            if (data[i].node_id == NODE_ID) {
                Serial.println("Time for current node to collect");
                data[i] = {
                    .id = ++message_id, .node_id = NODE_ID, .timestamp = 0, .type = 1, .value = 12345
                };
                Serial.print("Sending aggregate data containing IDs: ");
                for (int i=0; i<record_count; i++) {
                    Serial.print(data[i].node_id);
                    Serial.print(", ");
                }
                uint8_t next_node_id;
                if (i == record_count - 1) {
                    next_node_id = from_id;
                } else {
                    next_node_id = data[i+1].node_id;
                }
                Serial.println("");
                if (send_multidata_data(data, record_count, next_node_id, from_id)) {
                    Serial.print("Forwarded data to node: ");
                    Serial.println(next_node_id, DEC);
                    Serial.println("");
                }
            } else {
                break; // there is still an uncollected node before current node
            }
        } else {
            // continue through to find the first 0 id  
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
    MultidataMessage *_msg = (MultidataMessage*)recv_buf;
    if (msg_type == MESSAGE_TYPE_NO_MESSAGE) {
        // Do nothing
    } else if (msg_type == MESSAGE_TYPE_CONTROL) {
        _handle_control_message(_msg, len, from, dest, receive_time);
    } else if (msg_type == MESSAGE_TYPE_DATA) {
        _handle_data_message();
    } else {
        Serial.print("WARNING: Received unexpected Message type: ");
        Serial.print(msg_type, DEC);
        Serial.print(" from ID: ");
        Serial.println(from);
    }
    release_recv_buffer();
} /* check_incoming_message */

/* END OF RECEIVE FUNCTIONS */

/* **** TEST SPECIFIC FUNCTIONS **** */

void send_aggregate_data_init() {
    if (known_nodes[0] <= 0) {
        Serial.println("No known nodes. NOT sending data request.");
        return;
    }
    Data data[MAX_DATA_RECORDS];

    /* for testing struct size:
    for (int i=0; i<MAX_DATA_RECORDS; i++) {
        data[i] = { .id = 0, .node_id = i+1, .timestamp = 0, .type = AGGREGATE_DATA_INIT, .value = 0 };
    }
    uint8_t num_data_records = MAX_DATA_RECORDS; */
    /* NOTE: above code is just for testing data struct size */
    uint8_t num_data_records = 0;
    for (int i=0; i<MAX_DATA_RECORDS && known_nodes[i] != 0; i++) {
        data[i] = {
            .id = 0, .node_id = known_nodes[i], .timestamp = 0, .type = 0, .value = 0 };
        Serial.print(known_nodes[i], DEC);
        Serial.print(" ");
        num_data_records++;
    }
    Serial.println("");
    if (send_multidata_data(data, num_data_records, known_nodes[0])) {
        Serial.print("-- Sent data: AGGREGATE_DATA_INIT to ID: ");
        Serial.println(known_nodes[0], DEC);
    } else {
        Serial.println("ERROR: did not successfully send aggregate data collection request");
        remove_known_node_id(known_nodes[0]);
    }
} /* send_aggregate_data_init */

void send_control_next_request_time(unsigned long timeout)
{
    Control control = { .id = ++message_id,
          .code = CONTROL_NEXT_REQUEST_TIME, .from_node = NODE_ID, .data = 5000 };
    memcpy(control.nodes, known_nodes, MAX_CONTROL_NODES);
    Serial.println("Broadcasting next request time");
    if (send_multidata_control(&control, RH_BROADCAST_ADDRESS)) {
        Serial.println("-- Sent control: CONTROL_NEXT_REQUEST_TIME");
    } else {
        Serial.println("ERROR: did not successfully broadcast aggregate data collection request");
    }
} /* send_next_aggregate_data_request */

void test_aggregate_data_collection_with_sleep()
{
    static unsigned long last_collection_request_time = 0;
    static long next_collection_time = -1;
    int collection_delay = 2000;
    if (next_collection_time > 0 && millis() >= next_collection_time + collection_delay) {
        send_aggregate_data_init();
        next_collection_time = -1;
    } else if (millis() - last_collection_request_time > 30000) {
        unsigned long t = 10000;
        send_control_next_request_time(t);
        last_collection_request_time = millis();
        next_collection_time = millis() + 5000;
    }
}; /* test_aggregate_data_collection */

/* END OF TEST FUNCTIONS */

/* **** SETUP and LOOP **** */

void setup()
{
    while (!Serial);
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
    Serial.print("Node ID: ");
    Serial.println(NODE_ID);
    router = new RHMesh(radio, NODE_ID);
    //rf95.setModemConfig(RH_RF95::Bw125Cr48Sf4096);
    if (!router->init())
        Serial.println("Router init failed");
    Serial.print(F("FREQ: ")); Serial.println(FREQ);
    if (!radio.setFrequency(FREQ)) {
        Serial.println("Radio frequency set failed");
    } 
    radio.setTxPower(TX, false);
    radio.setCADTimeout(CAD_TIMEOUT);
    router->setTimeout(TIMEOUT);
    Serial.println("");
    delay(100);
}

void loop()
{
    if (NODE_ID == COLLECTOR_NODE_ID) {
        switch (TEST_TYPE) {
            case AGGREGATE_DATA_COLLECTION_WITH_SLEEP_TEST:
                test_aggregate_data_collection_with_sleep();
                check_incoming_message();
                break;
            default:
                Serial.print("UNKNOWN TEST TYPE: ");
                Serial.println(TEST_TYPE);
        }
    } else {
        if (millis() >= next_listen) {
            if (millis() >= next_listen + 1000) {
                check_collection_state();
            }
            //if (millis() >= next_listen + 3000)
            check_incoming_message();
        }
    }
}

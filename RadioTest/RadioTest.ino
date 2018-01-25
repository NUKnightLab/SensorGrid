#include <RHMesh.h>
#include <RHRouter.h>
#include <RH_RF95.h>
#include <SPI.h>

/* SET THIS FOR EACH NODE */
#define NODE_ID 3 // 1 is collector; 2,3 are sensors

#define FREQ 915.00
#define TX 5
#define CAD_TIMEOUT 1000
#define TIMEOUT 1000
#define RF95_CS 8
#define REQUIRED_RH_VERSION_MAJOR 1
#define REQUIRED_RH_VERSION_MINOR 82
#define RF95_INT 3
#define COLLECTOR 0
#define SENSOR 1
#define MAX_MESSAGE_SIZE 255
#define NETWORK_ID 3
#define SENSORGRID_VERSION 1

/**
 * Overall max message size is somewhere between 244 and 247 bytes. 248 will cause invalid length error
 * 
 * Note that these max sizes on the message structs are system specific due to struct padding. The values
 * here are specific to the Cortex M0
 * 
 */
#define MAX_DATA_RECORDS 40
#define MAX_CONTROL_NODES 237

// test types
#define BOUNCE_DATA_TEST 0
#define CONTROL_SEND_DATA_TEST 1
#define MULTIDATA_TEST 2
#define AGGREGATE_DATA_COLLECTION_TEST 3
#define TEST_TYPE AGGREGATE_DATA_COLLECTION_TEST

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
#define CONTROL_AGGREGATE_SEND_DATA 4

static RH_RF95 radio(RF95_CS, RF95_INT);
static RHMesh* router;
static uint8_t message_id = 0;

/* Defining list of nodes */
int sensorArray[2] = {2,3};
int node_type;

typedef struct Control {
    uint8_t id;
    uint8_t code;
    uint8_t from_node;
    uint8_t nodes[MAX_CONTROL_NODES];
};

typedef struct Data {
    uint8_t id; // 1-255 indicates Data$
    uint8_t node_id;
    uint8_t timestamp;
    uint8_t type;
    int16_t value;
};

typedef struct Message {
    uint8_t sensorgrid_version;
    uint8_t network_id;
    int8_t message_type;
    union {
      struct Control control;
      struct Data data;
    };
};

typedef struct MultidataMessage {
    uint8_t sensorgrid_version;
    uint8_t network_id;
    int8_t message_type;
    uint8_t len;
    union {
      struct Control control;
      struct Data data[MAX_DATA_RECORDS];
    };
};

uint8_t MAX_MESSAGE_PAYLOAD = sizeof(Data);
uint8_t MESSAGE_OVERHEAD = sizeof(Message) - MAX_MESSAGE_PAYLOAD;
uint8_t MAX_MULIDATA_MESSAGE_PAYLOAD = sizeof(MultidataMessage);
uint8_t MULTIDATA_MESSAGE_OVERHEAD = sizeof(MultidataMessage) - MAX_DATA_RECORDS * MAX_MESSAGE_PAYLOAD;

uint8_t recv_buf[MAX_MESSAGE_SIZE] = {0};
static bool recv_buffer_avail = true;

/* Collection state */
uint8_t uncollected_nodes[MAX_CONTROL_NODES] = {0};
Data aggregated_data[MAX_DATA_RECORDS*2] = {};
uint8_t aggregated_data_count = 0;
uint8_t collector_id;

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

uint8_t get_next_collection_node_id() {
    uint8_t _id = uncollected_nodes[0];
    if (_id > 0) {
        return _id;
    } else {
        return 1; // TODO: return to the collector that initiated the collection
    }
}

void remove_uncollected_node_id(uint8_t id) {
    int dest = 0;
    for (int i=0; i<MAX_CONTROL_NODES; i++) {
        if (uncollected_nodes[i] != id)
            uncollected_nodes[dest++] = uncollected_nodes[i];
    }
}

void add_aggregated_data_record(Data record) {
    bool found_record = false;
    for (int i=0; i<aggregated_data_count; i++) {
        if (aggregated_data[i].id == record.id && aggregated_data[i].node_id == record.node_id)
            found_record = true;
    }
    if  (!found_record) {
        memcpy(&aggregated_data[aggregated_data_count++], &record, sizeof(Data));
        remove_uncollected_node_id(record.node_id);
    }
}
/* END OF UTILS */

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


int8_t _receive_message(uint16_t timeout=NULL, uint8_t* source=NULL, uint8_t* dest=NULL, uint8_t* id=NULL, uint8_t* flags=NULL)
{
    uint8_t len = MAX_MESSAGE_SIZE;
    if (!recv_buffer_avail) {
        Serial.println("WARNING: Could not initiate receive message. Receive buffer is locked.");
        return MESSAGE_TYPE_NONE_BUFFER_LOCK;
    }
    Message* _msg;
    lock_recv_buffer(); // lock to be released by calling client
    if (timeout) {
        if (router->recvfromAckTimeout(recv_buf, &len, timeout, source, dest, id, flags)) {
            _msg = (Message*)recv_buf;
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
            validate_recv_buffer(len);
            Serial.print("Received buffered message. len: "); Serial.print(len, DEC);
            Serial.print("; type: "); print_message_type(_msg->message_type); Serial.println("");
            return _msg->message_type;
        } else {
            return MESSAGE_TYPE_NO_MESSAGE;
        }
    } else {
        if (router->recvfromAck(recv_buf, &len, source, dest, id, flags)) {
            _msg = (Message*)recv_buf;
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
            validate_recv_buffer(len);
            Serial.print("Received buffered message. len: "); Serial.print(len, DEC);
            Serial.print("; type: "); print_message_type(_msg->message_type); Serial.println("");
            return _msg->message_type;
        } else {
            return MESSAGE_TYPE_NO_MESSAGE;
        }
    }
}

int8_t receive(uint8_t* source=NULL, uint8_t* dest=NULL, uint8_t* id=NULL, uint8_t* flags=NULL)
{
    return _receive_message(NULL, source, dest, id, flags);
}

int8_t receive(uint16_t timeout, uint8_t* source=NULL, uint8_t* dest=NULL, uint8_t* id=NULL, uint8_t* flags=NULL)
{
    return _receive_message(timeout, source, dest, id, flags);
}

Control get_control_from_buffer()
{
    if (recv_buffer_avail) {
        Serial.println("WARNING: Attempt to extract control from unlocked buffer");
    }
    int8_t message_type = ((Message*)recv_buf)->message_type;
    if ( message_type != MESSAGE_TYPE_CONTROL) {
        Serial.print("WARNING: Attempt to extract control from non-control type: ");
        Serial.println(message_type, DEC);
    }
    return ((Message*)recv_buf)->control;
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

Data get_data_from_buffer()
{
    if (recv_buffer_avail) {
        Serial.println("WARNING: Attempt to extract data from unlocked buffer");
    }
    int8_t message_type = ((Message*)recv_buf)->message_type;
    if (message_type != MESSAGE_TYPE_DATA) {
        Serial.print("WARNING: Attempt to extract data from non-data type: ");
        Serial.println(message_type, DEC);
    }
    return ((Message*)recv_buf)->data;
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

/* END OF RECEIVE FUNCTIONS */

/* **** SEND FUNCTIONS **** */

bool send_message(uint8_t* msg, uint8_t len, uint8_t toID)
{
    Serial.print("Sending message type: ");
    print_message_type(((Message*)msg)->message_type);
    Serial.print("; length: ");
    Serial.println(len, DEC);
    unsigned long start = millis();
    uint8_t err = router->sendtoWait(msg, len, toID);
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

bool send_control(Control control, uint8_t dest)
{
    Message msg = {
        .sensorgrid_version = SENSORGRID_VERSION,
        .network_id = NETWORK_ID,
        .message_type = MESSAGE_TYPE_CONTROL
    };
    msg.control = control;
    uint8_t len = sizeof(Control) + MESSAGE_OVERHEAD;
    return send_message((uint8_t*)&msg, len, dest);
}

bool send_data(Data data, uint8_t dest)
{
    Message msg = {
        .sensorgrid_version = SENSORGRID_VERSION,
        .network_id = NETWORK_ID,
        .message_type = MESSAGE_TYPE_DATA
    };
    msg.data = data;
    uint8_t len = sizeof(Data) + MESSAGE_OVERHEAD;
    return send_message((uint8_t*)&msg, len, dest);
}

bool send_multidata_control(Control *control, uint8_t dest)
{
    MultidataMessage msg = {
        .sensorgrid_version = SENSORGRID_VERSION,
        .network_id = NETWORK_ID,
        .message_type = MESSAGE_TYPE_CONTROL,
        .len = 1
    };
    memcpy(&msg.control, control, sizeof(Control));
    uint8_t len = sizeof(Control) + MULTIDATA_MESSAGE_OVERHEAD;
    return send_message((uint8_t*)&msg, len, dest);
}

bool send_multidata_data(Data *data, uint8_t array_size, uint8_t dest)
{
    MultidataMessage msg = {
        .sensorgrid_version = SENSORGRID_VERSION,
        .network_id = NETWORK_ID,
        .message_type = MESSAGE_TYPE_DATA,
        .len = array_size,
    };
    memcpy(msg.data, data, sizeof(Data)*array_size);
    uint8_t len = sizeof(Data)*array_size + MULTIDATA_MESSAGE_OVERHEAD;
    return send_message((uint8_t*)&msg, len, dest);
}

/* END OF SEND  FUNCTIONS */


/* **** TEST SPECIFIC FUNCTIONS **** */

void send_next_data_for_bounce()
{
    static int sensor_index = 1;
    sensor_index = !sensor_index;
    Data data = {
        .id = ++message_id,
        .node_id = 10,
        .timestamp = 12345,
        .type = 111,
        .value = 123
    };
    unsigned long hashof_sent = hash((uint8_t*)&data, sizeof(Data));
    Serial.print("Sending Message ID: "); Serial.print(data.id, DEC);
    Serial.print("; dest: "); Serial.print(sensorArray[sensor_index]);
    Serial.print("; hash: "); Serial.println(hashof_sent);
    if (send_data(data, sensorArray[sensor_index])) {
        Serial.println("-- Sent data. Waiting for return data. Will timeout in 5 seconds.");
        uint8_t from;
        int8_t msg_type = receive(5000, &from);
        if (msg_type == MESSAGE_TYPE_DATA) {
            Data _data = get_data_from_buffer();
            unsigned long _hash = hash((uint8_t*)&_data, sizeof(Data));
            Serial.print("Received return data from: "); Serial.print(from, DEC);
            Serial.print("; hash: "); Serial.print(_hash);
            Serial.print("; Message ID: "); Serial.println( _data.id, DEC);
            if (_hash != hashof_sent) {
                Serial.print("WARNING: Hash of received data does not match hash of sent. Sent: ");
                Serial.print(hashof_sent, DEC);
                Serial.print("; Received: ");
                Serial.println(_hash, DEC);
            }
            if (_data.id != message_id) {
                Serial.print("WARNING: Received message ID does not match sent message ID. Sent: ");
                Serial.print(message_id, DEC);
                Serial.print("; Received: ");
                Serial.println(_data.id, DEC);
            }
        } else {
            Serial.print("RECEIVED NON-DATA MESSAGE TYPE: ");
            Serial.println(msg_type, DEC);
        }
        release_recv_buffer();
    }
} /* send_next_data_for_bounce */

void receive_data_to_bounce() {
    uint8_t from;
    int8_t msg_type = receive(&from);
    if (msg_type == MESSAGE_TYPE_NO_MESSAGE) {
        // no-op
    } else if (msg_type == MESSAGE_TYPE_DATA) {
        Data _data = get_data_from_buffer();
        unsigned long _hash = hash((uint8_t*)&_data, sizeof(Data));
        Serial.print("Received message from: "); Serial.print(from, DEC);
        Serial.print("; hash: "); Serial.print(_hash);
        Serial.print("; Message ID: "); Serial.println(_data.id, DEC);
        if (send_data(_data, from)) {
            Serial.println("Returned data");
            Serial.println("");
        }
    } else {
            Serial.print("RECEIVED NON-DATA MESSAGE TYPE: ");
            Serial.print(msg_type, DEC);
            Serial.println("");
    }
    release_recv_buffer();
} /* receive_data_to_bounce */

void send_next_control_send_data() {
    static int sensor_index = 1;
    sensor_index = !sensor_index;
    Control control = {
        .id = ++message_id,
        .code = CONTROL_SEND_DATA
    };
    Serial.print("Sending Message ID: "); Serial.print(control.id, DEC);
    Serial.print("; dest: "); Serial.println(sensorArray[sensor_index]);
    if (send_control(control, sensorArray[sensor_index])) {
        Serial.println("-- Sent control. Waiting for return data. Will timeout in 5 seconds.");
        uint8_t from;
        int8_t msg_type = receive(5000, &from);
        if (msg_type == MESSAGE_TYPE_DATA) {
            Data _data = get_data_from_buffer();
            Serial.print("Received return data from: "); Serial.print(from, DEC);
            Serial.print("; Message ID: "); Serial.println( _data.id, DEC);
        } else {
            Serial.print("RECEIVED NON-DATA MESSAGE TYPE: ");
            Serial.println(msg_type, DEC);
        }
        release_recv_buffer();
    }
} /* send_next_control_send_data */

void receive_control_send_data()
{
    uint8_t from;
    int8_t msg_type = receive(&from);
    if (msg_type == MESSAGE_TYPE_NO_MESSAGE) {
        // no-op
    } else if (msg_type == MESSAGE_TYPE_CONTROL) {
        Control _control = get_control_from_buffer();
        Serial.print("Received control message from: "); Serial.print(from, DEC);
        Serial.print("; Message ID: "); Serial.println(_control.id, DEC);
        if (_control.code == CONTROL_SEND_DATA) {
            Data data = {
              .id = ++message_id,
              .node_id = NODE_ID,
              .timestamp = 12345,
              .type = 111,
              .value = 123
            };
            if (send_data(data, from)) {
                Serial.println("Returned data");
                Serial.println("");
            }
        } else {
            Serial.print("Received unexpected CONTROL code: ");
            Serial.println(_control.code, DEC);
            Serial.println("");
        }
    } else {
            Serial.print("RECEIVED NON-CONTROL MESSAGE TYPE: ");
            Serial.print(msg_type, DEC);
            Serial.println("");
    }
    release_recv_buffer();
} /* receive_control_send_data */

void send_next_multidata_control() {
    Serial.println("Sending next multidata control");
    static int sensor_index = 1;
    sensor_index = !sensor_index;
    Control control = { .id = ++message_id,
          .code = CONTROL_SEND_DATA };
    Serial.print("; dest: "); Serial.println(sensorArray[sensor_index]);
    if (send_multidata_control(&control, sensorArray[sensor_index])) {
        Serial.println("-- Sent control. Waiting for return data. Will timeout in 5 seconds.");
        uint8_t from;
        int8_t msg_type = receive(5000, &from);
        uint8_t num_records;
        if (msg_type == MESSAGE_TYPE_DATA) {
            Serial.print("Received return data from: "); Serial.println(from, DEC);
            Serial.print("RECORD IDs:");
            Data* _data = get_multidata_data_from_buffer(&num_records);
            for (int record_i=0; record_i<num_records; record_i++) {
                Serial.print(" ");
                Serial.print( _data[record_i].id, DEC);
            }
            Serial.println("");
        } else {
            Serial.print("RECEIVED NON-DATA MESSAGE TYPE: ");
            Serial.println(msg_type, DEC);
        }
        release_recv_buffer();
    }
} /* send_next_multidata_control */

void receive_multidata_control()
{
    uint8_t from;
    int8_t msg_type = receive(&from);
    if (msg_type == MESSAGE_TYPE_NO_MESSAGE) {
        // no-op
    } else if (msg_type == MESSAGE_TYPE_CONTROL) {
        Control _control = get_multidata_control_from_buffer();
        Serial.print("Received control message from: "); Serial.print(from, DEC);
        Serial.print("; Message ID: "); Serial.println(_control.id, DEC);
        if (_control.code == CONTROL_NONE) {
            Serial.println("Received NO-OP control. Doing nothing");
        } else if (_control.code == CONTROL_SEND_DATA) {
            int NUM_DATA_RECORDS = MAX_DATA_RECORDS;
            Data data[NUM_DATA_RECORDS];
            for (int data_i=0; data_i<NUM_DATA_RECORDS; data_i++) {
                data[data_i] = {
                  .id = ++message_id,
                  .node_id = NODE_ID,
                  .timestamp = 12345,
                  .type = 111,
                  .value = 123 
                };
            }
            if (send_multidata_data(data, NUM_DATA_RECORDS, from)) {
                Serial.println("Returned data");
                Serial.println("");
            }
        } else {
            Serial.print("Received unexpected CONTROL code: ");
            Serial.println(_control.code, DEC);
            Serial.println("");
        }
        print_ram();
    } else {
            Serial.print("RECEIVED NON-CONTROL MESSAGE TYPE: ");
            Serial.print(msg_type, DEC);
            Serial.println("");
    }
    release_recv_buffer();
} /* receive_control_send_data */

void send_next_aggregate_data_request()
{
    //unsigned long start_time = millis();
    Control control = { .id = ++message_id,
          .code = CONTROL_AGGREGATE_SEND_DATA, .from_node = NODE_ID, .nodes = {2,3} };
    Serial.print("Broadcasting aggregate data request");
    if (send_multidata_control(&control, RH_BROADCAST_ADDRESS)) {
        Serial.println("-- Sent control. Waiting for return data.");
    } else {
        Serial.println("ERROR: did not successfully broadcast aggregate data collection request");
    }
} /* send_next_aggregate_data_request */

void listen_for_aggregate_data_response()
{
    uint8_t from;
    int8_t msg_type = receive(&from);
    if (msg_type == MESSAGE_TYPE_NO_MESSAGE) {
        // do nothing
    } else if (msg_type == MESSAGE_TYPE_DATA) {
        Serial.print("Received data from IDs: ");
        uint8_t len;
        Data* _data_array = get_multidata_data_from_buffer(&len);
        for (int i=0; i<len; i++) {
            Serial.print(_data_array[i].node_id);
            Serial.print(", ");
        }
    }
    release_recv_buffer();
} /* listen_for_aggregate_data_response */

void test_aggregate_data_collection()
{
    static unsigned long last_collection_request_time = 0;
    if (millis() - last_collection_request_time > 30000) {
        send_next_aggregate_data_request();
        last_collection_request_time = millis();
    } else {
        listen_for_aggregate_data_response();
    }
}; /* test_aggregate_data_collection */

void check_collection_state() {
    if (aggregated_data_count >= MAX_DATA_RECORDS) {
        // too much aggregated data. send to collector
        Serial.print("Sending aggregate data containing IDs: ");
        for (int i=0; i<MAX_DATA_RECORDS; i++) {
            Serial.print(aggregated_data[i].node_id);
            Serial.print(", ");
        }
        Serial.println("");
        if (send_multidata_data(aggregated_data, MAX_DATA_RECORDS, collector_id)) {
            Serial.print("Forwarded aggregated data to collector node: ");
            Serial.println(collector_id, DEC);
            Serial.println("");
            memset(aggregated_data, 0, MAX_DATA_RECORDS*sizeof(Data));
            memcpy(aggregated_data, &aggregated_data[MAX_DATA_RECORDS], MAX_DATA_RECORDS*sizeof(Data));
            memset(&aggregated_data[MAX_DATA_RECORDS], 0, MAX_DATA_RECORDS*sizeof(Data));
            aggregated_data_count = aggregated_data_count - MAX_DATA_RECORDS;
        }
    } else if (get_next_collection_node_id() == NODE_ID) {
        Serial.println("Identified self as next in collection order.");
        remove_uncollected_node_id(NODE_ID);
        uint8_t next_node_id = get_next_collection_node_id();
        Serial.print("Sending data to: ");
        Serial.println(next_node_id, DEC);
        // TODO: potential array overrun here
        Data _data = {
          .id = ++message_id,
          .node_id = NODE_ID,
          .timestamp = 12345,
          .type = 111,
          .value = 123 
        };
        aggregated_data[aggregated_data_count++] = _data;
        Serial.print("Sending aggregate data containing IDs: ");
        for (int i=0; i<aggregated_data_count; i++) {
            Serial.print(aggregated_data[i].node_id);
            Serial.print(", ");
        }
        Serial.println("");
        if (send_multidata_data(aggregated_data, aggregated_data_count, next_node_id)) {
            Serial.println("Forwarded data to node: ");
            Serial.println(next_node_id, DEC);
            Serial.println("");
            memset(aggregated_data, 0, sizeof(aggregated_data));
            aggregated_data_count = 0;
        }
    }
}
void receive_aggregate_data_request()
{
    uint8_t from;
    int8_t msg_type = receive(&from);
    if (msg_type == MESSAGE_TYPE_NO_MESSAGE) {
        // Do nothing
    } else if (msg_type == MESSAGE_TYPE_CONTROL) {
        Control _control = get_multidata_control_from_buffer();
        Serial.print("Received control message from: "); Serial.print(from, DEC);
        Serial.print("; Message ID: "); Serial.println(_control.id, DEC);
        if (_control.code == CONTROL_NONE) {
          Serial.println("Received NO-OP control. Doing nothing");
        } 
        else if (_control.code == CONTROL_AGGREGATE_SEND_DATA) {
            memcpy(uncollected_nodes, _control.nodes, MAX_CONTROL_NODES);
            collector_id = _control.from_node;
            Serial.print("Received collection control with node IDs: ");
            for (int node_id=0; node_id<MAX_CONTROL_NODES && _control.nodes[node_id] > 0; node_id++) {
                Serial.print(_control.nodes[node_id]);
                Serial.print(", ");
            }
            Serial.println("\n");
        }
        bool OVERFILL_AGGREGATE_DATA_BUFFER = true;
        if (OVERFILL_AGGREGATE_DATA_BUFFER) {
            for (int i=0; i<MAX_DATA_RECORDS; i++) {
                Data data = { .id=i, .node_id=i+10, .timestamp=12345, .type=1, .value=123 };
                add_aggregated_data_record(data);
            }
        }
    } else if (msg_type == MESSAGE_TYPE_DATA) {
        uint8_t len;
        Data* _data_array = get_multidata_data_from_buffer(&len);
        Serial.print("Received data array of length: ");
        Serial.println(len, DEC);
        for (int i=0; i<len; i++) {
            add_aggregated_data_record(_data_array[i]);
        }
    } else {
        Serial.print("WARNING: Reived unexpected Message type: ");
        Serial.println(msg_type, DEC);
    }
    release_recv_buffer();
} /* receive_aggregate_data_request */

/* END OF TEST FUNCTIONS */

/* **** SETUP and LOOP **** */

void setup() {
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
    if (NODE_ID == 1) {
        node_type = COLLECTOR;
    } else {
        node_type = SENSOR;
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

void loop() {

    if (node_type == COLLECTOR) {
        switch (TEST_TYPE) {
            case BOUNCE_DATA_TEST:
                send_next_data_for_bounce();
                break;
            case CONTROL_SEND_DATA_TEST:
                send_next_control_send_data();
                break;
            case MULTIDATA_TEST:
                send_next_multidata_control();
                break;
            case AGGREGATE_DATA_COLLECTION_TEST:
                test_aggregate_data_collection();
                break;
            default:
                Serial.print("UNKNOWN TEST TYPE: ");
                Serial.println(TEST_TYPE);
        }
    } else if (node_type == SENSOR) {
        switch (TEST_TYPE) {
            case BOUNCE_DATA_TEST:
                receive_data_to_bounce();
                break;
            case CONTROL_SEND_DATA_TEST:
                receive_control_send_data();
                break;
            case MULTIDATA_TEST:
                receive_multidata_control();
                break;
            case AGGREGATE_DATA_COLLECTION_TEST:
                check_collection_state();
                receive_aggregate_data_request();
                break;
            default:
                Serial.print("UNKNOWN TEST TYPE: ");
                Serial.println(TEST_TYPE);
        }
    }
}
  

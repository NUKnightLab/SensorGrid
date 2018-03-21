#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "protocol.h"

//

typedef void (*PrintRecordFunction)(uint8_t* record);
typedef int (*SerializeRecordFunction)(uint8_t* record, char* str, size_t len);


typedef struct DataType_s
{
    uint8_t type;
    uint8_t size;
    PrintRecordFunction print_fcn;
    SerializeRecordFunction serialize_fcn;
} DataType;

void print_battery_level(uint8_t* record)
{
    _BATTERY_LEVEL* r = (_BATTERY_LEVEL*)record;
    p(F("BATTERY_LEVEL: %d\n"), r->value);
}

int serialize_battery_level(uint8_t* record, char* str, size_t len)
{
    _BATTERY_LEVEL* r = (_BATTERY_LEVEL*)record;
    return snprintf(str, len, "{\"type\":\"BATTERY_LEVEL\",\"value\":%d}", r->value);
}


void print_sharp_gp2y1010au0f(uint8_t* record)
{
    _SHARP_GP2Y1010AU0F* r = (_SHARP_GP2Y1010AU0F*)record;
    p(F("SHARP_GP2Y1010AU0F VAL: %d; TIMESTAMP: %d\n"), r->value, r->timestamp);
}

int serialize_sharp_gp2y1010au0f(uint8_t* record, char* str, size_t len)
{
    _SHARP_GP2Y1010AU0F* r = (_SHARP_GP2Y1010AU0F*)record;
    return snprintf(str, len,
        "{\"type\":\"SHARP_GP2Y1010AU0F\",\"value\":%d,\"timestamp\":%d}",
        r->value, r->timestamp);
}

void print_warn_50_pct_data_history(uint8_t* record)
{
    p(F("WARN_50_PCT_DATA_HISTORY\n"));
}

int serialize_warn_50_pct_data_history(uint8_t* record, char* str, size_t len)
{
    return snprintf(str, len, "{\"type\":\"WARN_50_PCT_DATA_HISTORY\"}");
}

DataType data_types[] = {
    { DATA_TYPE_BATTERY_LEVEL,
      sizeof(_BATTERY_LEVEL),
      &print_battery_level,
      &serialize_battery_level},
    { DATA_TYPE_SHARP_GP2Y1010AU0F,
      sizeof(_SHARP_GP2Y1010AU0F),
      &print_sharp_gp2y1010au0f,
      &serialize_sharp_gp2y1010au0f},
    { DATA_TYPE_WARN_50_PCT_DATA_HISTORY,
      sizeof(_WARN_50_PCT_DATA_HISTORY),
      &print_warn_50_pct_data_history,
      &serialize_warn_50_pct_data_history}
};


bool remove_collection_list_node(_COLLECTION_LIST* list, uint8_t node_id)
{
    uint8_t index = 0;
    bool removed = false;
    for (int i=0; i<list->node_count; i++) {
        if (list->nodes[i].node_id == node_id) {
            list->node_count--;
            removed = true;
        } else {
            list->nodes[index].node_id = list->nodes[i].node_id;
            list->nodes[index].prev_record_id = list->nodes[i].prev_record_id;
            index++;
        }
    }
    return removed;
}

uint8_t copy_data(uint8_t* data, uint8_t* buffer, uint8_t* len,
            uint8_t remove_node_id)
{
    /* returns the ID associated with node removed from collection if it exists */
    uint8_t r = 0;
    // node_id, message_id, record_count, type
    memcpy(data, buffer, 4);
    uint8_t data_index = 4;
    uint8_t buffer_index = 4;
    if (buffer[3] == DATA_TYPE_NODE_COLLECTION_LIST) {
        data_index++; // skip the ID for now
        uint8_t node_count = buffer[buffer_index++];
        for (int i=0; i<buffer[4]; i++) {
            if (buffer[buffer_index] == remove_node_id) {
                buffer_index++;
                r = buffer[buffer_index++];
                node_count--;
            } else {
                memcpy(&data[data_index], &buffer[buffer_index], 2);
                data_index += 2;
                buffer_index += 2;
            }
        }
        data[4] = node_count;
    }
    p(F("memcpy %d bytes from buffer_index %d to data_index %d"),
        *len-buffer_index, buffer_index, data_index);
    if (buffer_index < *len) {
        memcpy(&data[data_index], &buffer[buffer_index], *len-buffer_index);
    }
    *len = data_index + *len - buffer_index;
    return r;
}

void to_bytes(NewRecordSet* set, uint8_t* bytes, uint8_t* len)
{
    if (*len < 3) return;
    memcpy(bytes, set, 3); // node_id, message_id, record_count
    uint8_t bytes_index = 3;
    uint8_t data_index = 0;
    uint8_t full = 0;
    for (int i=0; i<set->record_count && !full; i++) {
        uint8_t type = set->data[data_index];
        uint8_t size;
        if (type == DATA_TYPE_NODE_COLLECTION_LIST) {
            _COLLECTION_LIST* list = (_COLLECTION_LIST*)&set->data[data_index];
            size = 2 + 2 * list->node_count;
        } else {
            for (int j=0; j<sizeof(data_types); j++){
                if (type == data_types[j].type) {
                    size = data_types[j].size;
                    break;
                }
            }
        }
        if (size + bytes_index < *len) {
            memcpy(&bytes[bytes_index], &set->data[data_index], size);
            bytes_index += size;
            data_index+= size;
        } else {
            full = 1;
        }
    }
    *len = bytes_index;
}

void from_bytes(NewRecordSet* set, uint8_t* bytes, uint8_t* len)
{
    memcpy(set, bytes, 3); // node_id, message_id, record_count
    uint8_t bytes_index = 3;
    uint8_t data_index = 0;
    for (int i=0; i<set->record_count; i++) {
        uint8_t type = bytes[bytes_index];
        uint8_t size;
        if (type == DATA_TYPE_NODE_COLLECTION_LIST) {
            _COLLECTION_LIST* list = (_COLLECTION_LIST*)&bytes[bytes_index];
            size = 2 + 2 * list->node_count;
        } else {
            for (int j=0; j<sizeof(data_types); j++){
                if (type == data_types[j].type) {
                    size = data_types[j].size;
                    break;
                }
            }
        }
        memcpy(&set->data[data_index], &bytes[bytes_index], size);
        bytes_index += size;
        data_index += size;
    }
    *len = bytes_index;
}

void print_record_set(NewRecordSet* newset, uint8_t* size)
{
    uint8_t index = 0;
    p(F("Printing record set NODE_ID: %d, RECORD_COUNT: %d\n"),
        newset->node_id, newset->record_count);
    for (int i=0; i<newset->record_count; i++) {
        uint8_t type = newset->data[index];
        if (type == DATA_TYPE_NODE_COLLECTION_LIST) {
            p(F("COLLECTION_LIST: "));
            _COLLECTION_LIST* list = (_COLLECTION_LIST*)&newset->data[index];
            for (int n=0; n<list->node_count; n++) {
                output(F("[%d, %d] "), list->nodes[n].node_id,
                    list->nodes[n].prev_record_id);
            }
            output(F("\n"));
            index += (2 + 2 * list->node_count);
        } else if (type == DATA_TYPE_NEXT_ACTIVITY_SECONDS) {
            p(F("NEXT_ACTIVITY_SECONDS: "));
            _next_activity_seconds* record = (_next_activity_seconds*)&(newset->data[index]);
            output(F("%d\n"), record->value);
            index += sizeof(_next_activity_seconds);
        } else {
            for (int j=0; j<sizeof(data_types); j++){
                if (type == data_types[j].type) {
                    data_types[j].print_fcn(&newset->data[index]);
                    index += data_types[j].size;
                    break;
                }
            }
        }
    }
    *size = sizeof(NewRecordSet) + index;
}

void _extract_record_set(NewRecordSet* newset, uint8_t* size, char* buf, size_t* buflen)
{
    uint8_t index = 0;
    p(F("Extracting record set NODE_ID: %d, RECORD_COUNT: %d\n"),
        newset->node_id, newset->record_count);
    if (newset->node_id == config.node_id) {
        /* skip through the collection record and set the size */
        p(F("Skipping JSON rendering for collection list.\n"));
        if (newset->record_count == 1) {
            uint8_t type = newset->data[index];
            if (type == DATA_TYPE_NODE_COLLECTION_LIST) {
                p(F("COLLECTION_LIST: "));
                _COLLECTION_LIST* list = (_COLLECTION_LIST*)&newset->data[index];
                for (int n=0; n<list->node_count; n++) {
                    output(F("[%d, %d] "), list->nodes[n].node_id,
                        list->nodes[n].prev_record_id);
                }
                output(F("\n"));
                index += (2 + 2 * list->node_count);
            } else {
                p(F("** WARNING: Non-collection list record type for collector ID\n"));
            }
        } else {
            p(F("** WARNING: bad record count for record with collector ID\n"));
        }
        *size = sizeof(NewRecordSet) + index;
        return;
    }
/*
    JsonObject& node_data = sensor_data.createNestedObject();
    if (!node_data.success()) {
        p(F("Node data object creation was not successful"));
        return;
    }
    JsonArray& data_array = node_data.createNestedArray("data");
    if (!data_array.success()) {
        p(F("data array creation was not successful"));
        return;
    }
*/
    const int JSON_BUFFER_SIZE = 500;
    char json[JSON_BUFFER_SIZE];
    int json_index = snprintf(json, JSON_BUFFER_SIZE, "{node:%d,data:[", newset->node_id);
    //uint8_t json_index = 0;
    p(F("Successfully created node_data object and array\n"));
    //node_data["node_id"] = newset->node_id;
    //sensor_data.printTo(Serial);
    //Serial.println("");
    for (int i=0; i<newset->record_count; i++) {
        uint8_t type = newset->data[index];
        if (type == DATA_TYPE_NODE_COLLECTION_LIST) {
            p(F("** WARNING: theoretical unreachable condition - collection list\n"));
            /*
            p(F("COLLECTION_LIST: "));
            _COLLECTION_LIST* list = (_COLLECTION_LIST*)&newset->data[index];
            for (int n=0; n<list->node_count; n++) {
                output(F("[%d, %d] "), list->nodes[n].node_id,
                    list->nodes[n].prev_record_id);
            }
            output(F("\n"));
            index += (2 + 2 * list->node_count);
            */
        } else if (type == DATA_TYPE_NEXT_ACTIVITY_SECONDS) {
            p("index: %d; data[index]: %d\n", index, newset->data[index]);
            p("index: %d; data[index+1]: %d\n", index, newset->data[index+1]);
            p("index: %d; data[index+2]: %d\n", index, newset->data[index+2]);
            p(F("NEXT_ACTIVITY_SECONDS: "));
            _next_activity_seconds* record = (_next_activity_seconds*)&(newset->data[index]);
            output(F("%d\n"), record->value);
            index += sizeof(_next_activity_seconds);
        } else {
            for (int j=0; j<sizeof(data_types); j++){
                if (type == data_types[j].type) {
                    //char str[100];
                    //data_types[j].serialize_fcn(&newset->data[index], str, 100);
                    //data_array.add(str);
                    int sz = data_types[j].serialize_fcn(&newset->data[index],
                        &json[json_index], JSON_BUFFER_SIZE - json_index);
                    if (sz >= 0 && sz < JSON_BUFFER_SIZE - json_index) {
                        //data_types[j].print_fcn(&newset->data[index]);
                        index += data_types[j].size;
                        json_index += sz;
                        break;
                    } else {
                        p(F("Full JSON buffer:\n"));
                        json[json_index-1] = ']';
                        json[json_index++] = '}';
                        json[json_index] = '\0';
                        p(json);
                        output("\n");
                        json_index = 0;
                        i--; // redo this loop with cleared buffer
                    }
                }
            }
        }
        if (i < newset->record_count) {
            json[json_index++] = ',';
        } else {
            json[json_index++] = ']';
        }
    }
    //sensor_data.printTo(Serial);
    Serial.println(json);
    *size = sizeof(NewRecordSet) + index;
}

void print_records(uint8_t* data, uint8_t len)
{
    uint8_t index = 0;
    uint8_t size;
    p("Printing records for data stream: ");
    for (int i=0; i<len; i++) output("%d ", data[i]);
    output("\n");
    while (len > 0) {
        print_record_set((NewRecordSet*)&data[index], &size);
        index += size;
        len -= size;
    };
}

/*
void _extract_records(uint8_t* data, uint8_t len)
{
    uint8_t index = 0;
    uint8_t size;
    static char* apibuf[100];
    static size_t apibuflen = 100;
    static uint8_t apibufindex = 0;
    p("Extracting records for data stream: ");
    for (int i=0; i<len; i++) output("%d ", data[i]);
    output("\n");
    do {
        extract_record_set((NewRecordSet*)&data[index], &size, (char*)&apibuf[apibufindex], &apibuflen);
        index += size;
        len -= size;
    } while (len > 0);
}
*/


int serialize_record_set(char* buf, size_t* buflen, NewRecordSet* rs,
        size_t* rslen, bool* buffer_full)
{
    static const int min_serialization_size = sizeof("{\"type\":\"X\"}");
    uint8_t rs_index = 0;
    int buf_index = sprintf(buf, "{\"node_id\":%d,\"data\":[", rs->node_id);
    size_t _buflen = *buflen - buf_index - 1;
    size_t _buflen_orig = _buflen;
    *buffer_full = false;
    for (int i=0; i<rs->record_count; i++) {
        for (int j=0; j<sizeof(data_types); j++){
            if (rs->data[rs_index] == data_types[j].type) {
                int charlen = data_types[j].serialize_fcn(&rs->data[rs_index],
                    &buf[buf_index], _buflen);
                if (charlen < _buflen - 1) {
                    buf_index += charlen;
                    _buflen = _buflen - charlen;
                    rs_index += data_types[j].size;
                    buf[buf_index++] = ',';
                } else {
                    *buffer_full = true;
                }
                break;
            }
        }
        if (*buffer_full) {
            if (_buflen == _buflen_orig) {
                *rslen = 0;
                return 0;
            }
            break;
        }
    }
    buf[buf_index-1] = ']';
    buf[buf_index++] = '}';
    *buflen = _buflen;
    *rslen = sizeof(NewRecordSet) + rs_index;
    return buf_index;
}

int serialize_records(char* buf, size_t buflen, uint8_t* data, size_t datalen)
{
    int data_index = 0;
    size_t size;
    int buf_index = sprintf(buf, "{\"data\":[");
    buflen -= (buf_index + 3); // reserve buffer for end characters
    bool buffer_full = false;
    while (buflen && data_index < datalen) {
        int added_chars = serialize_record_set(&buf[buf_index], &buflen,
            (NewRecordSet*)&data[data_index], &size, &buffer_full);
        if (added_chars) {
            buf_index += added_chars;
            data_index += size;
            buf[buf_index++] = ',';
        }
        if (buffer_full) break;
    };
    if (data_index > 0) buf_index--; // remove trailing comma
    buf[buf_index++] = ']';
    buf[buf_index++] = '}';
    buf[buf_index++] = '\0';
    return data_index;
}

void record_record_set_received_id(NewRecordSet* record_set, uint8_t* size)
{
    uint8_t index = 0;
    p("Recording received id: [%d, %d]\n", record_set->node_id, record_set->message_id);
    received_record_ids[record_set->node_id] = record_set->message_id;
    for (int i=0; i<record_set->record_count; i++) {
        for (int j=0; j<sizeof(data_types); j++){
            if (record_set->data[index] == data_types[j].type) {
                index += data_types[j].size;
                break;
            }
        }
    }
    *size = sizeof(NewRecordSet) + index;
}


void record_received_ids(uint8_t* data, int len)
{
    uint8_t index = 0;
    uint8_t size;
    while (len > 0) {
        record_record_set_received_id((NewRecordSet*)&data[index], &size);
        index += size;
        len -= size;
    }
}

int extract_records(uint8_t* buf, uint8_t* data, uint8_t len)
{
    NewRecordSet* record_set = (NewRecordSet*)data;
    _COLLECTION_LIST* list = (_COLLECTION_LIST*)(record_set->data);
    uint8_t index = sizeof(NewRecordSet) + 2 + list->node_count * sizeof(_collect_node);
    memcpy(buf, &data[index], len - index);
    record_received_ids(&data[index], len - index);
    return len - index;
}

/* struct initializers */

void add_record(NewRecordSet* record_set, uint8_t* record, uint8_t* index)
{
    uint8_t type = record[0];
    uint8_t size;
    for (int i=0; i<sizeof(data_types); i++) {
        if (type == data_types[i].type) {
            size = data_types[i].size;
            break;
        }
    }
    memcpy(&record_set->data[*index], record, size);
    *index += size;
    record_set->record_count++;
}

/**
 * Write a collection request to the start of the message buffer. Writes
 * directly to buffer of length len. Length must be at least long enough
 * to write a RecordSet of collection size 0, or nothing is written. *len
 * is replaced with the number of bytes written to the buffer (i.e. the
 * current writeable index location of the buffer.
 *
 * The written record set will contain a collection request with a node count
 * of the number of nodes added to the request. This will be however many
 * of the requested nodes fit into the buffer (taking 2 bytes each)
 *
 * Althgough there is basic support here for doing otherwise, in practice, a
 * collection request comes first among record sets, and the collection request
 * record set will only contain that one record for that node, which is
 * the collector node.
 */
void create_collection_record(uint8_t node_id, uint8_t message_id,
        uint8_t* nodes, uint8_t node_count, uint8_t* buffer, uint8_t* len)
{
    if (*len < 5) {
        *len = 0;
        return;
    }
    uint8_t index = 0;
    buffer[index++] = node_id;
    buffer[index++] = message_id;
    buffer[index++] = 1; // currently only a single record from a node requesting collection
    buffer[index++] = DATA_TYPE_NODE_COLLECTION_LIST;
    uint8_t node_count_index = index;
    buffer[index++] = 0;
    for (int i=0; i<node_count && index<(*len-1); i++) {
        buffer[index++] = nodes[i];
        buffer[index++] = received_record_ids[nodes[i]];
        buffer[node_count_index]++;
    }
    *len = index;
}

void create_next_activity_record(uint8_t node_id, uint8_t message_id,
        uint16_t seconds, uint8_t* buffer, uint8_t* len)
{
    NewRecordSet* set = (NewRecordSet*)buffer;
    set->node_id = node_id;
    set->message_id = message_id;
    set->record_count = 1;
    _next_activity_seconds* record = (_next_activity_seconds*)set->data; 
    record->type = DATA_TYPE_NEXT_ACTIVITY_SECONDS;
    record->value = seconds;
    *len = sizeof(NewRecordSet) + sizeof(_next_activity_seconds);
}

int _main(int argc, char** argv)
{

    /* intiate a message buffer with a collection list request */

    uint8_t node_list[] = {2, 4};
    uint8_t len = MAX_MESSAGE_SIZE;
    uint8_t buffer[MAX_MESSAGE_SIZE];
    uint8_t buf_index = 0;
    create_collection_record(1, 100, node_list, sizeof(node_list), buffer, &len);
    p(F("Init buffer w/ col request NODE_ID: 1; MSG_ID: 100; NODES: {2, 4}\n"));
    buf_index = len;
    len = MAX_MESSAGE_SIZE - buf_index;

    /* data record set from node 2 */
    NewRecordSet set = {
        .node_id = 2, .message_id = 200, .record_count = 0
    };
    uint8_t record_index = 0;

    /* sharp dust sample */
/*
    _SHARP_GP2Y1010AU0F record1 = _sharp_gp2y1010au0f;
    record1.value = 123;
    record1.timestamp = 123456;
    add_record(&set, (uint8_t*)&record1, &record_index);
*/

    /* another sharp dust sample */
/*
    _SHARP_GP2Y1010AU0F record2 = _sharp_gp2y1010au0f;
    record2.value = 789;
    record2.timestamp = 654321;
    add_record(&set, (uint8_t*)&record2, &record_index);
*/

    /* battery level */
/*
    _BATTERY_LEVEL record3 = _battery_level;
    record3.value = 44;
    _WARN_50_PCT_DATA_HISTORY record4 = _warn_50_pct_data_history;
    add_record(&set, (uint8_t*)&record3, &record_index);
*/

    p(F("Created record set for Node %d with %d records\n"),
        set.node_id, set.record_count);

    /* copy the record set into the buffer after the collection request */
    to_bytes(&set, &buffer[buf_index], &len);
    buf_index = buf_index + len;
    len = MAX_MESSAGE_SIZE - buf_index;

    /* now do from bytes to check */
    len = buf_index;
    buf_index = 0;
    uint8_t record_set_size = 0;
    uint8_t size;
    do {
        NewRecordSet newset = {0};
        from_bytes(&newset, &buffer[buf_index], &record_set_size);
        print_record_set(&newset, &size);
        buf_index += record_set_size;
        len -= record_set_size;
    } while (len > 0);
}

/**
 *  adjust head of collection buffer to current data_index location
 *  re-writes the header if mid-record
 */
void trim_collection_buffer(uint8_t* buffer, int* current_index, int trim_index)
{
    int index = 0;
    int _index = 0;
    for (int i=0; i<*current_index; i++) output("%d ", buffer[i]);
    output("\n");
    while (index <= trim_index) {
        NewRecordSet* rs = (NewRecordSet*)&buffer[index];
        uint8_t record_count = rs->record_count;
        for (int i=0; index <= trim_index && i < rs->record_count; i++) {
            uint8_t type = rs->data[index];
            for (int j=0; j<sizeof(data_types); j++){
                if (type == data_types[j].type) {
                    index += data_types[j].size;
                    if (index <= trim_index) record_count--;
                    break;
                }
            }
        }
        if (index > trim_index) {
            buffer[_index++] = rs->node_id;
            buffer[_index++] = rs->message_id;
            buffer[_index++] = record_count;
        }
    };
    int size = *current_index - trim_index;
    memcpy(&buffer[_index], &buffer[trim_index], size);
    *current_index = _index + size;
    for (int i=0; i<*current_index; i++) output("%d ", buffer[i]);
    output("\n");
}

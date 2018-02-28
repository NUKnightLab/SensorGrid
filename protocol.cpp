#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "protocol.h"


//


typedef void (*PrintRecordFunction)(uint8_t* record);

typedef struct DataType_s
{
    uint8_t type;
    uint8_t size;
    PrintRecordFunction print_fcn;
} DataType;

void print_battery_level(uint8_t* record)
{
    _BATTERY_LEVEL* r = (_BATTERY_LEVEL*)record;
    p(F("BATTERY_LEVEL: %d\n"), r->value);
}

void print_sharp_gp2y1010au0f(uint8_t* record)
{
    _SHARP_GP2Y1010AU0F* r = (_SHARP_GP2Y1010AU0F*)record;
    p(F("SHARP_GP2Y1010AU0F VAL: %d; TIMESTAMP: %d\n"), r->value, r->timestamp);
}

void print_warn_50_pct_data_history(uint8_t* record)
{
    p(F("WARN_50_PCT_DATA_HISTORY\n"));
}

DataType data_types[] = {
    { DATA_TYPE_BATTERY_LEVEL, sizeof(_BATTERY_LEVEL), &print_battery_level },
    { DATA_TYPE_SHARP_GP2Y1010AU0F, sizeof(_SHARP_GP2Y1010AU0F), &print_sharp_gp2y1010au0f },
    { DATA_TYPE_WARN_50_PCT_DATA_HISTORY, sizeof(_WARN_50_PCT_DATA_HISTORY), &print_warn_50_pct_data_history }
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

void copy_data(uint8_t* data, uint8_t* buffer, uint8_t* len,
            uint8_t remove_node_id)
{

    // node_id, message_id, record_count, type
    memcpy(data, buffer, 4);
    uint8_t data_index = 4;
    uint8_t buffer_index = 4;
    if (buffer[3] == DATA_TYPE_NODE_COLLECTION_LIST) {
        data_index++; // skip the node count for now
        uint8_t node_count = buffer[buffer_index++];
        for (int i=0; i<buffer[4]; i++) {
            if (buffer[buffer_index] == remove_node_id) {
                buffer_index += 2;
                node_count--;
            } else {
                memcpy(&data[data_index], &buffer[buffer_index], 2);
                data_index += 2;
                buffer_index += 2;
            }
        }
        data[4] = node_count;
    }
    if (buffer_index < *len) {
        memcpy(&data[data_index], &buffer[buffer_index], *len-buffer_index);
    }
    *len = data_index;
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
                    data_types[j].print_fcn(&newset->data[index]);
                    index += data_types[j].size;
                    break;
                }
            }
        }
    }
    *size = sizeof(NewRecordSet) + index;
}

void print_records(uint8_t* data, uint8_t len)
{
    uint8_t index = 0;
    uint8_t size;
    p("Printing records for data stream: ");
    for (int i=0; i<len; i++) output("%d ", data[i]);
    output("\n");
    do {
        print_record_set((NewRecordSet*)&data[index], &size);
        index += size;
        len -= size;
    } while (len > 0);
}

/* struct initializers */

uint8_t received_record_ids[MAX_NODES];

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

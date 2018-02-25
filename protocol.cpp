#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "protocol.h"

#define MAX_MESSAGE_SIZE 240
#define MAX_NODES 100

/* Data types */
#define DATA_TYPE_NODE_COLLECTION_LIST 1
#define DATA_TYPE_NEXT_ACTIVITY_SECONDS 2
#define DATA_TYPE_BATTERY_LEVEL 3
#define DATA_TYPE_SHARP_GP2Y1010AU0F 4
#define DATA_TYPE_WARN_50_PCT_DATA_HISTORY 5
#define MAX_COLLECT_NODES 100

typedef struct __attribute__((packed)) CollectNodeStruct
{
    uint8_t node_id;
    uint8_t prev_record_id;
} collect_node;

struct __attribute__((packed)) COLLECTION_LIST_STRUCT
{
    uint8_t type;
    uint8_t node_count;
    struct CollectNodeStruct nodes[MAX_COLLECT_NODES];
} collection_list = { DATA_TYPE_NODE_COLLECTION_LIST };
typedef struct COLLECTION_LIST_STRUCT COLLECTION_LIST;

struct __attribute__((packed)) NEXT_ACTIVITY_SECONDS_STRUCT
{
    uint8_t type;
    uint16_t value;
} next_activity_seconds_struct = { DATA_TYPE_NEXT_ACTIVITY_SECONDS };
typedef struct NEXT_ACTIVITY_SECONDS_STRUCT next_activity_seconds;

struct __attribute__((packed)) BATTERY_LEVEL_STRUCT
{
    uint8_t type;
    uint8_t value;
} battery_level = { DATA_TYPE_BATTERY_LEVEL };
typedef struct BATTERY_LEVEL_STRUCT BATTERY_LEVEL;

struct __attribute__((packed)) SHARP_GP2Y1010AU0F_STRUCT
{
    uint8_t type;
    uint16_t value;
    uint32_t timestamp;
} sharp_gp2y1010au0f = { DATA_TYPE_SHARP_GP2Y1010AU0F };
typedef struct SHARP_GP2Y1010AU0F_STRUCT SHARP_GP2Y1010AU0F;

struct __attribute__((packed)) WARN_50_PCT_DATA_HISTORY_STRUCT
{
    uint8_t type;
} warn_50_pct_data_history = { DATA_TYPE_WARN_50_PCT_DATA_HISTORY };
typedef struct WARN_50_PCT_DATA_HISTORY_STRUCT WARN_50_PCT_DATA_HISTORY;

//

typedef struct __attribute__((packed)) RecordSet_s
{
    uint8_t node_id;
    uint8_t message_id;
    uint8_t record_count;
    uint8_t data[100];
} RecordSet;


typedef void (*PrintRecordFunction)(uint8_t* record);

typedef struct DataType_s
{
    uint8_t type;
    uint8_t size;
    PrintRecordFunction print_fcn;
} DataType;

void print_battery_level(uint8_t* record)
{
    BATTERY_LEVEL* r = (BATTERY_LEVEL*)record;
    printf("BATTERY_LEVEL: %d\n", r->value);
}

void print_sharp_gp2y1010au0f(uint8_t* record)
{
    SHARP_GP2Y1010AU0F* r = (SHARP_GP2Y1010AU0F*)record;
    printf("SHARP_GP2Y1010AU0F VAL: %d; TIMESTAMP: %d\n", r->value, r->timestamp);
}

void print_warn_50_pct_data_history(uint8_t* record)
{
    printf("WARN_50_PCT_DATA_HISTORY\n");
}

DataType data_types[] = {
    { DATA_TYPE_BATTERY_LEVEL, sizeof(BATTERY_LEVEL), &print_battery_level },
    { DATA_TYPE_SHARP_GP2Y1010AU0F, sizeof(SHARP_GP2Y1010AU0F), &print_sharp_gp2y1010au0f },
    { DATA_TYPE_WARN_50_PCT_DATA_HISTORY, sizeof(WARN_50_PCT_DATA_HISTORY), &print_warn_50_pct_data_history }
};

void to_bytes(RecordSet* set, uint8_t* bytes, uint8_t* len)
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
            COLLECTION_LIST* list = (COLLECTION_LIST*)&set->data[data_index];
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

void from_bytes(RecordSet* set, uint8_t* bytes, uint8_t* len)
{
    memcpy(set, bytes, 3); // node_id, message_id, record_count
    uint8_t bytes_index = 3;
    uint8_t data_index = 0;
    for (int i=0; i<set->record_count; i++) {
        uint8_t type = bytes[bytes_index];
        uint8_t size;
        if (type == DATA_TYPE_NODE_COLLECTION_LIST) {
            COLLECTION_LIST* list = (COLLECTION_LIST*)&bytes[bytes_index];
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

void print_record_set(RecordSet* newset)
{
    uint8_t index = 0;
    printf("\nPrinting record set NODE_ID: %d, RECORD_COUNT: %d\n",
        newset->node_id, newset->record_count);
    for (int i=0; i<newset->record_count; i++) {
        uint8_t type = newset->data[index];
        if (type == DATA_TYPE_NODE_COLLECTION_LIST) {
            printf("COLLECTION_LIST: ");
            COLLECTION_LIST* list = (COLLECTION_LIST*)&newset->data[index];
            for (int n=0; n<list->node_count; n++) {
                printf("[%d, %d] ", list->nodes[n].node_id,
                    list->nodes[n].prev_record_id);
            }
            printf("\n");
            index += (2 + 2 * list->node_count);
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
}

/* struct initializers */

uint8_t received_record_ids[MAX_NODES];

void add_record(RecordSet* record_set, uint8_t* record, uint8_t* index)
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


int _main(int argc, char** argv)
{

    /* intiate a message buffer with a collection list request */

    uint8_t node_list[] = {2, 4};
    uint8_t len = MAX_MESSAGE_SIZE;
    uint8_t buffer[MAX_MESSAGE_SIZE];
    uint8_t buf_index = 0;
    create_collection_record(1, 100, node_list, sizeof(node_list), buffer, &len);
    printf("Init buffer w/ col request NODE_ID: 1; MSG_ID: 100; NODES: {2, 4}\n");
    buf_index = len;
    len = MAX_MESSAGE_SIZE - buf_index;

    /* data record set from node 2 */
    RecordSet set = {
        .node_id = 2, .message_id = 200, .record_count = 0
    };
    uint8_t record_index = 0;

    /* sharp dust sample */
    SHARP_GP2Y1010AU0F record1 = sharp_gp2y1010au0f;
    record1.value = 123;
    record1.timestamp = 123456;
    add_record(&set, (uint8_t*)&record1, &record_index);

    /* another sharp dust sample */
    SHARP_GP2Y1010AU0F record2 = sharp_gp2y1010au0f;
    record2.value = 789;
    record2.timestamp = 654321;
    add_record(&set, (uint8_t*)&record2, &record_index);

    /* battery level */
    BATTERY_LEVEL record3 = battery_level;
    record3.value = 44;
    WARN_50_PCT_DATA_HISTORY record4 = warn_50_pct_data_history;
    add_record(&set, (uint8_t*)&record3, &record_index);

    printf("Created record set for Node %d with %d records\n",
        set.node_id, set.record_count);

    /* copy the record set into the buffer after the collection request */
    to_bytes(&set, &buffer[buf_index], &len);
    buf_index = buf_index + len;
    len = MAX_MESSAGE_SIZE - buf_index;

    /* now do from bytes to check */
    len = buf_index;
    buf_index = 0;
    uint8_t record_set_size = 0;
    do {
        RecordSet newset = {0};
        from_bytes(&newset, &buffer[buf_index], &record_set_size);
        print_record_set(&newset);
        buf_index += record_set_size;
        len -= record_set_size;
    } while (len > 0);
}

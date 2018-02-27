#ifndef _PROTOCOL_H
#define _PROTOCOL_H

#include "util.h"

#define MAX_MESSAGE_SIZE 240
#define MAX_NODES 100

/* Data types */
#define DATA_TYPE_NODE_COLLECTION_LIST 1
#define DATA_TYPE_NEXT_ACTIVITY_SECONDS 2
#define DATA_TYPE_BATTERY_LEVEL 3
#define DATA_TYPE_SHARP_GP2Y1010AU0F 4
#define DATA_TYPE_WARN_50_PCT_DATA_HISTORY 5
#define MAX_COLLECT_NODES 100

typedef struct __attribute__((packed)) NewRecordSet
{
    uint8_t node_id;
    uint8_t message_id;
    uint8_t record_count;
    uint8_t data[];
    //uint8_t data[100];
};

void create_collection_record(uint8_t node_id, uint8_t message_id,
        uint8_t* nodes, uint8_t node_count, uint8_t* buffer, uint8_t* len);
void create_next_activity_record(uint8_t node_id, uint8_t message_id,
        uint16_t seconds, uint8_t* buffer, uint8_t* len);
void from_bytes(NewRecordSet* set, uint8_t* bytes, uint8_t* len);
void print_record_set(NewRecordSet* newset);

typedef struct __attribute__((packed)) _CollectNodeStruct
{
    uint8_t node_id;
    uint8_t prev_record_id;
} _collect_node;

struct __attribute__((packed)) _COLLECTION_LIST_STRUCT
{
    uint8_t type;
    uint8_t node_count;
    struct _CollectNodeStruct nodes[MAX_COLLECT_NODES];
};
//} _collection_list = { DATA_TYPE_NODE_COLLECTION_LIST };
typedef struct _COLLECTION_LIST_STRUCT _COLLECTION_LIST;


struct __attribute__((packed)) _NEXT_ACTIVITY_SECONDS_STRUCT
{
    uint8_t type;
    uint16_t value;
};
//} _next_activity_seconds_struct = { DATA_TYPE_NEXT_ACTIVITY_SECONDS };
typedef struct _NEXT_ACTIVITY_SECONDS_STRUCT _next_activity_seconds;

struct __attribute__((packed)) _BATTERY_LEVEL_STRUCT
{
    uint8_t type;
    uint8_t value;
};
//} _battery_level = { DATA_TYPE_BATTERY_LEVEL };
typedef struct _BATTERY_LEVEL_STRUCT _BATTERY_LEVEL;

struct __attribute__((packed)) _SHARP_GP2Y1010AU0F_STRUCT
{
    uint8_t type;
    uint16_t value;
    uint32_t timestamp;
};
//} _sharp_gp2y1010au0f = { DATA_TYPE_SHARP_GP2Y1010AU0F };
typedef struct _SHARP_GP2Y1010AU0F_STRUCT _SHARP_GP2Y1010AU0F;

struct __attribute__((packed)) _WARN_50_PCT_DATA_HISTORY_STRUCT
{
    uint8_t type;
};
//} _warn_50_pct_data_history = { DATA_TYPE_WARN_50_PCT_DATA_HISTORY };
typedef struct _WARN_50_PCT_DATA_HISTORY_STRUCT _WARN_50_PCT_DATA_HISTORY;

void to_bytes(NewRecordSet* set, uint8_t* bytes, uint8_t* len);
void add_record(NewRecordSet* record_set, uint8_t* record, uint8_t* index);
bool remove_collection_list_node(_COLLECTION_LIST* list, uint8_t node_id);
void copy_data(uint8_t* data, uint8_t* buffer, uint8_t* len,
            uint8_t remove_node_id);

#endif



#ifndef _PROTOCOL_H
#define _PROTOCOL_H

typedef struct __attribute__((packed)) NewRecordSet
{
    uint8_t node_id;
    uint8_t message_id;
    uint8_t record_count;
    uint8_t data[100];
};

void create_collection_record(uint8_t node_id, uint8_t message_id,
        uint8_t* nodes, uint8_t node_count, uint8_t* buffer, uint8_t* len);
void from_bytes(NewRecordSet* set, uint8_t* bytes, uint8_t* len);
void print_record_set(NewRecordSet* newset);

#endif

//
// Created by Yuriy Kulinchenko on 01/08/2025.
//

#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"
#include "memory.h"

typedef struct {
    ObjString* key;
    Value value;
} Entry;

typedef struct {
    int count;
    int capacity;
    Entry* entries;
} Table;

void initTable(Table* table);
void freeTable(Table* table);
bool tableSet(Table* table, ObjString* key, Value value);

#endif

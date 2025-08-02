#include <stdlib.h>
#include <string.h>
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75

void initTable(Table* table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void freeTable(Table* table) {
    FREE_ARRAY(Entry, table->entries, table->capacity);
    initTable(table);
}

void growCapacity(Table* table) {
    int oldCapacity = table->capacity;
    table->capacity = oldCapacity < 8 ? 8 :oldCapacity * 2;
    table->entries = GROW_ARRAY(Entry, table->entries, oldCapacity, table->capacity);
}

Entry* findEntry(Entry* entries, int capacity, ObjString* key) {
    int index = (int) key->hash % capacity;
    for (;;) {
        Entry* entry = &entries[index];
        if (entry->key == key || entry->key == NULL) {
            return entry;
        }
        index = (index + 1) % capacity;
    }
}

bool tableSet(Table* table, ObjString* key, Value value) {
    if (table->count + 1 > TABLE_MAX_LOAD * table->capacity) {
        growCapacity(table);
    }
    // Guaranteed to have space now:
    Entry* entry = findEntry(table->entries, table->capacity, key);
    bool isNewKey = entry->key == NULL;
    if (isNewKey) table->count++;

    entry->key = key;
    entry->value = value;
    return isNewKey;
}
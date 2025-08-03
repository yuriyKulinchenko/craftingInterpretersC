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

Entry* findEntry(Entry* entries, int capacity, ObjString* key) {
    // Returns entry pointer with key "key"
    // If this is not present, returns first empty slot
    // If any tombstones are encountered on the way, the tombstone is returned
    Entry* tombstone = NULL;
    int index = (int) key->hash % capacity;
    for (;;) {
        Entry* entry = &entries[index];
        bool isNull = entry->key == NULL && !AS_BOOL(entry->value); // Is not null if tombstone
        if (isNull || entry->key == key) { // BAD - uses reference equality
            return entry;
        }
        index = (index + 1) % capacity;
    }
}

void adjustCapacity(Table* table, int capacity) {
    Entry* entries = ALLOCATE(Entry, capacity);
    for (int i = 0; i < capacity; i++) {
        entries->key = NULL;
        entries->value = BOOL_VAL(false);
    }

    // Now, hashes must be recomputed, entries put in etc

    int count = 0;
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key == NULL) continue;

        Entry* position = findEntry(entries, capacity, entry->key);
        position->key = entry->key;
        position->value = entry->value;
        count++;
    }

    FREE_ARRAY(Entry, table->entries, table->capacity);
    table->entries = entries;
    table->capacity = capacity;
    table->count = count;
}

bool tableSet(Table* table, ObjString* key, Value value) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = table->capacity < 8 ? 8 : table->capacity * 2;
        adjustCapacity(table, capacity);
    }
    // Guaranteed to have space now:
    Entry* entry = findEntry(table->entries, table->capacity, key);
    bool isNewKey = entry->key == NULL;
    if (isNewKey) table->count++;

    entry->key = key;
    entry->value = value;
    return isNewKey;
}

bool tableGet(Table* table, ObjString* key, Value* value) {
    // If the value exists, it is stored in the given pointer location
    // Returns true if value is present, false otherwise
    if (table->count == 0) return false;

    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key != NULL) return false;

    *value = entry->value;
    return true;
}

void tableAddAll(Table* from, Table* to) {
    for (int i = 0; i < from->capacity; i++) {
        Entry* fromEntry = &from->entries[i];
        if (fromEntry->key != NULL) tableSet(to, fromEntry->key, fromEntry->value);
    }
}
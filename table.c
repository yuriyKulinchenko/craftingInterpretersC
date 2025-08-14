#include <stdlib.h>
#include <string.h>
#include "memory.h"
#include "object.h"
#include "table.h"

#include <stdio.h>

#include "value.h"

#define TABLE_MAX_LOAD 0.75
#define TABLE_DEBUG

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
    uint32_t index =  key->hash % capacity;
    for (;;) {
        Entry* entry = &entries[index];

        if (entry->key == NULL) {
            if (AS_BOOL(entry->value)) {
                tombstone = entry;
            } else {
                if (tombstone != NULL) return tombstone;
                return entry;
            }
        }

        if (entry->key == key) return entry;

        index = (index + 1) % capacity;
    }
}

void adjustCapacity(Table* table, int capacity) {
    Entry* entries = ALLOCATE(Entry, capacity);
    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = BOOL_VAL(false);
    }

    // Now, hashes must be recomputed, entries put in etc

    int count = 0;
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key == NULL) continue; // Skips tombstones as well

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
    if (isNewKey && !AS_BOOL(entry->value)) table->count++;
    entry->key = key;
    entry->value = value;
    return isNewKey;
}

bool tableGet(Table* table, ObjString* key, Value* value) {
    // If the value exists, it is stored in the given pointer location
    // Returns true if value is present, false otherwise
    if (table->count == 0) return false;

    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    if (value != NULL) *value = entry->value;
    return true;
}

void tableAddAll(Table* from, Table* to) {
    for (int i = 0; i < from->capacity; i++) {
        Entry* fromEntry = &from->entries[i];
        if (fromEntry->key != NULL) tableSet(to, fromEntry->key, fromEntry->value);
    }
}

bool tableDelete(Table* table, ObjString* key) {
    if (table->count == 0) return false;
    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;
    entry->key = NULL;
    entry->value = BOOL_VAL(true); // Tombstone added
    return true;
}

ObjString* tableFindString(Table* table, char* chars, int length, uint32_t hash) {
    // Return null if the string is not found
    // previous reference equality check does not work here
    if (table->count == 0) return NULL;
    uint32_t index = hash % table->capacity;
    for (;;) {
        Entry* entry = &table->entries[index];
        if (entry->key == NULL) {
            if (!AS_BOOL(entry->value)) return NULL;
        } else {
            if (entry->key == NULL && !AS_BOOL(entry->value)) return NULL;
            if (entry->key->hash == hash && entry->key->length == length &&
                memcmp(chars, entry->key->chars, length) == 0) return entry->key;
        }
        index = (index + 1) % table->capacity;
    }
}

void printTable(Table* table) {
    printf("{");
    bool isFirst = true;
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key == NULL) continue;

        if (isFirst) {
            isFirst = false;
        } else {
            printf(", ");
        }

        printf("%s -> ", entry->key->chars);
        printValue(entry->value);
    }
    printf("}");

#ifdef TABLE_DEBUG
    printf("\n");
    printf("===TABLE_DEBUG===\n");
    printf("count: %d, ", table->count);
    printf("capacity: %d", table->capacity);

    printf("\n[");
    isFirst = true;
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];

        if (isFirst) {
            isFirst = false;
        } else {
            printf(", ");
        }


        if (entry->key == NULL) {
            if (AS_BOOL(entry->value)) {
                printf("TOMBSTONE");
            } else {
                printf("NULL");
            }
        } else {
            printf("VALUE");
        }
    }
    printf("]\n");
    printf("===END_TABLE_DEBUG===\n");
#endif
}

void markTable(Table* table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key != NULL) {
            markObject((Obj*) entry->key);
            markValue(entry->value);
        }
    }
}

void tableRemoveWhite(Table* table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key == NULL) break;
        if (entry->key != NULL && !entry->key->obj.isMarked) {
            tableDelete(table, entry->key);
        }
    }
}

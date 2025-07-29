#include "value.h"
#include "memory.h"
#include <stdio.h>

void initValueArray(ValueArray* array) {
    array->count = 0;
    array->capacity = 0;
    array->values = NULL;
}


void writeValueArray(ValueArray* array, Value value) {
    if (array->capacity < array->count + 1) {
        const uint8_t oldCapacity = array->capacity;
        array->capacity = oldCapacity < 8 ? 8 : 2 * oldCapacity;
        array->values = GROW_ARRAY(Value, array->values, oldCapacity, array->capacity);
    }
    array->values[array->count++] = value;
}


void freeValueArray(ValueArray* array) {
    reallocate(array->values, array->capacity, 0);
}

void printValue(Value value) {
    switch (value.type) {
        case VAL_NUMBER: {
            printf("%g", AS_NUMBER(value));
            break;
        }
        case VAL_NIL: {
            printf("nil");
            break;
        }
        case VAL_BOOL: {
            printf(AS_BOOL(value) ? "true" : "false");
            break;
        }
    }
}
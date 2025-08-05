#include "value.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>

#include "object.h"

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

char* objToString(Obj* object) {
    char* chars;
    switch (object->type) {
        case OBJ_STRING: {
            asprintf(&chars, "\"%s\"",((ObjString*) object)->chars);
            break;
        }
        default: asprintf(&chars, "unrecognized object"); break;
    }
    return chars;
}

char* valueToString(Value value) {
    char* chars;
    switch (value.type) {
        case VAL_NUMBER: {
            asprintf(&chars,"%g", AS_NUMBER(value));
            break;
        }
        case VAL_NIL: {
            asprintf(&chars, "nil");
            break;
        }
        case VAL_BOOL: {
            asprintf(&chars, AS_BOOL(value) ? "true" : "false");
            break;
        }

        case VAL_OBJ: {
            return objToString(AS_OBJ(value));
        }

        default: asprintf(&chars, "unrecognized value"); break;
    }
    return chars;
}

void printValue(Value value) {
    char* chars = valueToString(value);
    printf(chars);
    free(chars);
}
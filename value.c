#include "value.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "object.h"

static uint8_t nextPower(uint8_t n) {
    if (n < 8) return 8;
    if ((n & n - 1) == 0) return n; // Check if n is a power of 2
    uint8_t shift = 1;
    uint8_t old;
    do {
        old = n;
        n |= n >> shift;
        shift <<= 1;
    } while (n != old);
    return n + 1;
}

void initValueArray(ValueArray* array) {
    array->count = 0;
    array->capacity = 0;
    array->values = NULL;
}

void initValueArrayCopy(ValueArray* array, Value* values, uint8_t count) {
    // values pointer is passed from VM stack
    if (count == 0) {
        initValueArray(array);
        return;
    }
    array->count = count;
    array->capacity = nextPower(count);
    array->values = ALLOCATE(Value, array->capacity);
    for (int i = 0; i < count; i++) {
        array->values[i] = values[i];
    }
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

char* arrayToString(ObjArray* array) {
    char* chars = strdup("[");
    bool isFirst = true;
    for (int i = 0; i < array->valueArray.count; i++) {
        if (!isFirst) {
            char* temp = chars;
            asprintf(&chars, "%s, ", temp);
            free(temp);
        } else {
            isFirst = false;
        }
        char* temp = chars;
        char* valueString = valueToString(array->valueArray.values[i]);
        asprintf(&chars, "%s%s", temp, valueString);
        free(temp);
        free(valueString);
    }

    char* temp = chars;
    asprintf(&chars, "%s]", temp);
    free(temp);
    return chars;
}

char* objToString(Obj* object) {
    char* returnChars;
    switch (object->type) {
        case OBJ_STRING: {
            ObjString* string = (ObjString*) object;
            char* chars = string->chars;
            asprintf(&returnChars, "\"%s\"", chars);
            break;
        }
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*) object;
            ObjFunction* function = closure->function;
            char* chars = function->name == NULL ? "script" : function->name->chars;
            asprintf(&returnChars, "<%s>", chars);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*) object;
            char* chars = function->name == NULL ? "script" : function->name->chars;
            asprintf(&returnChars, "<raw:%s>", chars);
            break;
        }
        case OBJ_ARRAY: {
            ObjArray* array = (ObjArray*) object;
            return arrayToString(array);
        }

        case OBJ_UPVALUE: {
            return strdup("upvalue");
        }

        case OBJ_CLASS: {
            ObjClass* klass = (ObjClass*) object;
            asprintf(&returnChars, "{class:%s}", klass->name->chars);
            break;
        }

        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*) object;
            asprintf(&returnChars, "{%s}", instance->klass->name->chars);
            break;
        }

        default: asprintf(&returnChars, "unrecognized object"); break;
    }
    return returnChars;
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

ObjString* valueKey(Value value) {
    // Returns an ObjString* which can be used as a key to uniquely represent a value in a table
    char* valueString = valueToString(value);
    char* returnString;
    char* prefix;
    switch (value.type) {
        case VAL_NIL: prefix = "NIL"; break;
        case VAL_NUMBER: prefix = "NUMBER"; break;
        case VAL_BOOL: prefix = "BOOL"; break;
        default: return NULL;
    }

    asprintf(&returnString, "%s:%s", prefix, valueString);
    free(valueString);
    return takeString(returnString, (int) strlen(returnString));
}

void printValue(Value value) {
    char* chars = valueToString(value);
    printf(chars);
    free(chars);
}

bool objEqual(Obj* aPtr, Obj* bPtr) {
    if (aPtr->type != bPtr->type) return false;

    switch (aPtr->type) {
        case OBJ_STRING: return aPtr == bPtr; // Reference equality now works because of interning
        case OBJ_FUNCTION: return false; // May change later
        default: return false;
    }
}

bool valuesEqual(Value a, Value b) {
    if (a.type != b.type) return false;

    switch (a.type) {
        case VAL_NIL: return true;
        case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_BOOL: return AS_BOOL(a) == AS_BOOL(b);
        case VAL_OBJ: return objEqual(AS_OBJ(a), AS_OBJ(b));
        default: return false;
    }
}
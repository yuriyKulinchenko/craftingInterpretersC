#include <stdio.h>
#include <string.h>
#include "memory.h"
#include "object.h"
#include "table.h"

#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, objectType) \
(type*)allocateObject(sizeof(type), objectType)

static Obj* allocateObject(size_t size, ObjType type) {
    Obj* object = reallocate(NULL, 0, size);
    object->type = type;

    object->next = vm.objects;
    vm.objects = object;

    return object;
}

static uint32_t hashString(const char* key, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= key[i];
        hash *= 16777619;
    }
    return hash;
}

static ObjString* allocateString(char* chars, int length, uint32_t hash) {
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    tableSet(&vm.strings, string, NIL_VAL);

    return string;
}

ObjString* takeString(char* chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
    if (interned != NULL) {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }


    return allocateString(chars, length, hash);
}

ObjString* copyString(char* chars, int length) {
    // Goal is to return an object class
    // If the string already exists, the reference to its duplicate is returned
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
    if (interned != NULL) return interned;
    // Do not assume ownership of chars, it cannot yet be freed


    char* copiedChars = ALLOCATE(char, length + 1);
    memcpy(copiedChars, chars, length);
    copiedChars[length] = '\0';
    return allocateString(copiedChars, length, hash);
}

ObjFunction* newFunction() {
    ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->upvalueCount = 0;
    function->name = NULL;
    initChunk(&function->chunk);
    return function;
}

ObjClosure* newClosure(ObjFunction* function) {
    ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
    closure->function = function;
    closure->upvalueCount = function->upvalueCount;
    closure->upvalues = ALLOCATE(ObjUpvalue*, closure->upvalueCount);
    for (int i = 0; i < closure->upvalueCount; i++) {
        closure->upvalues[i] = NULL;
    }
    return closure;
}

ObjUpvalue* newUpvalue(Value* value) {
    ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
    upvalue->location = value;
    return upvalue;
}

ObjArray* newArray(Value* values, uint8_t count) {
    ObjArray* array = ALLOCATE_OBJ(ObjArray, OBJ_ARRAY);
    initValueArrayCopy(&array->valueArray, values, count);
    return array;
}
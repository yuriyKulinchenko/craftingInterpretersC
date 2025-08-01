#include <stdio.h>
#include <string.h>
#include "memory.h"
#include "object.h"

#include "value.h"

#define ALLOCATE_OBJ(type, objectType) \
(type*)allocateObject(sizeof(type), objectType)

static Obj* allocateObject(size_t size, ObjType type) {
    Obj* object = reallocate(NULL, 0, size);
    object->type = type;

    object->next = vm.objects;
    vm.objects = object;

    return object;
}

static ObjString* allocateString(char* chars, int length) {
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    return string;
}

ObjString* takeString(char* chars, int length) {
    return allocateString(chars, length);
}

ObjString* copyString(const char* chars, int length) {
    // Goal is to return an object class
    char* copiedChars = ALLOCATE(char, length + 1);
    memcpy(copiedChars, chars, length);
    copiedChars[length] = '\0';
    return allocateString(copiedChars, length);
}
//
// Created by Yuriy Kulinchenko on 30/07/2025.
//

#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)
#define IS_STRING(value) isObjType(value, OBJ_STRING)

#define AS_STRING(value) ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)


typedef enum {
    OBJ_STRING,
} ObjType;


struct Obj {
    ObjType type;
};

struct ObjString { // Can be safely cast to Obj
    Obj obj;
    int length;
    char* chars;
};

ObjString* copyString(const char* chars, int length);

static inline bool isObjType(Value value, ObjType type) {
    if (!IS_OBJ(value)) return false;
    return OBJ_TYPE(value) == type;
}

#endif

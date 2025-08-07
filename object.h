//
// Created by Yuriy Kulinchenko on 30/07/2025.
//

#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "value.h"
#include "chunk.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_STRING(value) isObjType(value, OBJ_STRING)
#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)

#define AS_STRING(value) ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)
#define AS_FUNCTION(value) ((ObjFunction*)AS_OBJ(value))


typedef enum {
    OBJ_STRING,
    OBJ_FUNCTION
} ObjType;


struct Obj {
    ObjType type;
    Obj* next;
};

typedef struct {
    Obj obj;
    int arity;
    Chunk chunk;
    ObjString* name;
} ObjFunction;

struct ObjString { // Can be safely cast to Obj
    Obj obj;
    int length;
    char* chars;
    uint32_t hash;
};

ObjFunction* newFunction();

ObjString* takeString(char* chars, int length);
ObjString* copyString(char* chars, int length);

static inline bool isObjType(Value value, ObjType type) {
    if (!IS_OBJ(value)) return false;
    return OBJ_TYPE(value) == type;
}

#endif

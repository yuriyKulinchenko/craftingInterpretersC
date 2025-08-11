#include "memory.h"

#include <stdlib.h>

#include "object.h"
#include "vm.h"

void* reallocate(void* p, size_t oldSize, size_t newSize) {
    if (newSize == 0) {
        free(p);
        return NULL;
    }
    void* result = realloc(p, newSize);
    if (result == NULL) exit(1);
    return result;
}

#include <stdio.h>

static void freeObject(Obj* object) {
    switch (object->type) {
        case OBJ_STRING: {
            ObjString* string = (ObjString*) object;
            FREE_ARRAY(char, string->chars, string->length + 1);
            FREE(ObjString, object);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*) object;
            freeChunk(&function->chunk);
            FREE(ObjFunction, object);
            break;
        }

        case OBJ_ARRAY: {
            ObjArray* array = (ObjArray*) object;
            freeValueArray(&array->valueArray);
            FREE(ObjArray, object);
            break;
        }

        case OBJ_CLOSURE: {
            FREE(ObjClosure, object);
            break;
        }
    }
}

void freeObjects() {
    Obj* ptr = vm.objects;
    while (ptr != NULL) {
        Obj* nextPtr = ptr->next;
        freeObject(ptr);
        ptr = nextPtr;
    }
    vm.objects = NULL;
}
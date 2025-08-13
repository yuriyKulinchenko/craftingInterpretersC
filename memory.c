#include "memory.h"
#include "object.h"
#include "vm.h"

#include <stdlib.h>

void* reallocate(void* p, size_t oldSize, size_t newSize) {
    if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
    collectGarbage();
#endif
    }

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
            ObjClosure* closure = (ObjClosure*) object;
            FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);
            FREE(ObjClosure, object);
            break;
        }

        case OBJ_UPVALUE: {
            FREE(ObjUpvalue, object);
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

void collectGarbage() {
#ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
#endif


#ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
#endif
}
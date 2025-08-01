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

static void freeObject(Obj* object) {
    switch (object->type) {
        case OBJ_STRING: {
            ObjString * string = (ObjString*) object;
            FREE_ARRAY(char, string->chars, string->length + 1);
            FREE(ObjString, object);
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
}
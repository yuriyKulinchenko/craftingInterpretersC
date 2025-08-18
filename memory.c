#include "memory.h"
#include "object.h"
#include "vm.h"
#include "compiler.h"

#include <stdio.h>
#include <stdlib.h>

#define GC_HEAP_GROW_FACTOR 2

void* reallocate(void* p, size_t oldSize, size_t newSize) {
    vm.bytesAllocated += newSize - oldSize;

    if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
    collectGarbage();
#endif
        if (vm.bytesAllocated > vm.nextGC) {
            collectGarbage();
        }
    }

    if (newSize == 0) {
        free(p);
        return NULL;
    }
    void* result = realloc(p, newSize);
    if (result == NULL) exit(1);
    return result;
}

static void freeObject(Obj* object) {
#ifdef DEBUG_LOG_GC
    printf("    free: %s\n", valueToString(OBJ_VAL(object)));
#endif
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

        case OBJ_CLASS: {
            ObjClass* klass = (ObjClass*) object;
            freeTable(&klass->methods);
            FREE(ObjClass, object);
            break;
        }

        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*) object;
            freeTable(&instance->fields);
            FREE(ObjInstance, object);
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
    free(vm.grayStack);
}



void markObject(Obj* object) {
    // Marking an object adds it to the gray stack
    if (object == NULL) return;
    if (object->isMarked) return;
#ifdef DEBUG_LOG_GC
    printf("    mark: %s\n", valueToString(OBJ_VAL(object)));
#endif
    object->isMarked = true;

    if (vm.grayCount + 1 > vm.grayCapacity) {
        int oldCapacity = vm.grayCapacity;
        vm.grayCapacity = oldCapacity < 8 ? 8 : 2 * oldCapacity;
        Obj** newPtr = realloc(vm.grayStack, sizeof(Obj*) * vm.grayCapacity);
        if (newPtr == NULL) {
            free(vm.grayStack);
            exit(1);
        }
        vm.grayStack = newPtr;
    }

    vm.grayStack[vm.grayCount++] = object;
}

void markValue(Value value) {
    // Primitive values cannot be marked
    if (!IS_OBJ(value)) return;
    markObject(AS_OBJ(value));
}

static void markRoots() {
    for (Value* value = vm.stack; value < vm.stackTop; value++) {
        markValue(*value);
    }

    for (int i = 0; i < vm.frameCount; i++) {
        markObject((Obj*) vm.frames[i].closure);
    }

    for (ObjUpvalue* upvalue = vm.openUpvalues; upvalue != NULL; upvalue = upvalue->next) {
        markObject((Obj*) upvalue);
    }

    markTable(&vm.globals);
    // markTable(&vm.strings);
    markCompilerRoots();
}

static void blackenObject(Obj* object) {
    // The object has already been moved out of the gray stack
    // Mark all objects reachable that are accessible through 'object'
#ifdef DEBUG_LOG_GC
    printf("    blacken: %s\n", valueToString(OBJ_VAL(object)));
#endif
    switch (object->type) {

        case OBJ_FUNCTION: {
            const ObjFunction* function = (ObjFunction*) object;
            markObject((Obj*)function->name);
            for (int i = 0; i < function->chunk.constants.count; i++) {
                markValue(function->chunk.constants.values[i]);
            }
            break;
        }

        case OBJ_ARRAY: {
            const ObjArray* array = (ObjArray*) object;
            for (int i = 0; i < array->valueArray.count; i++) {
                const Value value = array->valueArray.values[i];
                markValue(value);
            }
            break;
        }

        case OBJ_CLOSURE: {
            const ObjClosure* closure = (ObjClosure*) object;
            markObject((Obj*)closure->function);
            for (int i = 0; i < closure->upvalueCount; i++) {
                markObject((Obj*)closure->upvalues[i]);
            }
            break;
        }

        case OBJ_UPVALUE: {
            const ObjUpvalue* upvalue = (ObjUpvalue*) object;
            markValue(upvalue->closed);
            break;
        }

        case OBJ_CLASS: {
            ObjClass* klass = (ObjClass*) object;
            markObject((Obj*)klass->name);
            markTable(&klass->methods);
            break;
        }

        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*) object;
            markObject((Obj*)instance->klass);
            markTable(&instance->fields);
            break;
        }

        default: break;
    }
}

static void trackReferences() {
    while (vm.grayCount > 0) {
        Obj* object = vm.grayStack[vm.grayCount - 1];
        vm.grayCount--;
        blackenObject(object);
    }
}

void sweep() {
    Obj* previous = NULL;
    Obj* current = vm.objects;
    while (current != NULL) {
#ifdef DEBUG_LOG_GC
#endif
        if (current->isMarked) {
            current->isMarked = false;
            previous = current;
            current = current->next;
        } else {
            Obj* unreachable = current;
            current = current->next;
            if (previous != NULL) {
                previous->next = current;
            } else {
                vm.objects = current;
            }
            freeObject(unreachable);
        }
    }
}

void collectGarbage() {
#ifdef DEBUG_LOG_GC
    printf("-- GC begin\n");
    size_t oldAmount = vm.bytesAllocated;
#endif

    markRoots();
    trackReferences();
    tableRemoveWhite(&vm.strings);
    sweep();

    vm.nextGC = vm.bytesAllocated * GC_HEAP_GROW_FACTOR;
#ifdef DEBUG_LOG_GC
    printf("-- GC end\n");
    printf("-- GC freed %d bytes of memory\n", (int) (oldAmount - vm.bytesAllocated));
#endif
}
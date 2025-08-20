#include "vm.h"
#include "common.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "compiler.h"
#include "memory.h"
#include "object.h"

VM vm;

void resetStack() {
    vm.stackTop = vm.stack;
    vm.frameCount = 0;
    vm.openUpvalues = NULL;
}

static void runtimeError(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    CallFrame* frame = &vm.frames[vm.frameCount - 1];
    size_t instruction = frame->ip - frame->closure->function->chunk.code - 1;
    int line = frame->closure->function->chunk.lines[instruction];
    fprintf(stderr, "[line %d] in script\n", line);
    resetStack();
}

void initVM() {
    resetStack();
    vm.objects = NULL;

    vm.grayCapacity = 0;
    vm.grayCount = 0;
    vm.grayStack = NULL;

    vm.bytesAllocated = 0;
    vm.nextGC = 1024 * 1024;

    initTable(&vm.strings);
    initTable(&vm.globals);
    vm.initString = copyString("init", 4);
}

void freeVM() {
    freeObjects();
    freeTable(&vm.strings);
    freeTable(&vm.globals);
}

void push(Value value) {
    *vm.stackTop = value;
    vm.stackTop++;
}

Value pop() {
    vm.stackTop--;
    return *vm.stackTop;
}

Value popCount(uint8_t n) {
    vm.stackTop -= n;
    return *vm.stackTop;
}

Value peek(int n) {
    return vm.stackTop[-1 - n];
}

static bool isFalsey(Value value) {
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

void concatenate() {
    Value aValue = peek(1);
    Value bValue = peek(0);
    ObjString* aString;
    ObjString* bString;

    if (IS_NUMBER(aValue)) {
        char* chars = valueToString(aValue);
        aString = takeString(chars, (int) strlen(chars));
    } else {
        aString = AS_STRING(aValue);
    }

    if (IS_NUMBER(bValue)) {
        char* chars = valueToString(bValue);
        bString = takeString(chars, (int) strlen(chars));
    } else {
        bString = AS_STRING(bValue);
    }

    int length = aString->length + bString->length;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, aString->chars, aString->length);
    memcpy(chars + aString->length, bString->chars, bString->length);
    chars[length] = '\0';
    ObjString* result = takeString(chars, length);
    pop();
    pop();
    push(OBJ_VAL(result));
}

bool addFrame(ObjClosure* closure, uint8_t argumentCount) {
    if (argumentCount != closure->function->arity) {
        runtimeError("Incorrect number of arguments passed into function");
        return false;
    }
    CallFrame* frame = &vm.frames[vm.frameCount++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
    frame->slots = vm.stackTop - argumentCount - 1;
    return true;
}

void popFrame() {
    // The stackTop must be reset
    vm.stackTop = vm.frames[vm.frameCount - 1].slots;
    vm.frameCount--;
}

static ObjUpvalue* captureUpvalue(Value* local) {
    ObjUpvalue* prevUpvalue = NULL;
    ObjUpvalue* upvalue = vm.openUpvalues;

    while (upvalue != NULL && upvalue->location > local) {
        // vm.openUpvalues is sorted, so traversal like this is possible:
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    // Now, upvalue->location <= local

    if (upvalue != NULL && upvalue->location == local) {
        return upvalue;
    }

    ObjUpvalue* createdUpvalue = newUpvalue(local);
    createdUpvalue->next = upvalue;

    if (prevUpvalue == NULL) {
        vm.openUpvalues = createdUpvalue;
    } else {
        prevUpvalue->next = createdUpvalue;
    }

    return createdUpvalue;
}

static void closeUpvalue(Value* value) {
    while (vm.openUpvalues != NULL && vm.openUpvalues->location >= value) {
        ObjUpvalue* upvalue = vm.openUpvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm.openUpvalues = upvalue->next;
        }
}

static bool setProperty(Value instanceValue, ObjString* propertyName, Value value) {
    if (!IS_INSTANCE(instanceValue)) {
        runtimeError("Can only set property of instance");
        return false;
    }
    ObjInstance* instance = AS_INSTANCE(instanceValue);
    tableSet(&instance->fields, propertyName, value);
    return true;
}

static bool bindMethod(ObjClass* klass, ObjString* name, Value receiver, Value* value) {
    Value methodValue;
    if (!tableGet(&klass->methods, name, &methodValue)) return false;
    ObjClosure* method = AS_CLOSURE(methodValue);

    ObjBoundMethod* boundMethod = newBoundMethod(receiver, method);

    *value = OBJ_VAL(boundMethod);
    return true;
}

static bool getProperty(Value instanceValue, ObjString* propertyName, Value* value) {
    if (IS_ARRAY(instanceValue)) {
        if (propertyName->length != 6 || memcmp(propertyName->chars, "length", 6) != 0) {
            runtimeError("Can only access length property of array");
            return false;
        }
        ObjArray* array = AS_ARRAY(instanceValue);
        *value = NUMBER_VAL(array->valueArray.count);
        return true;
    }

    if (!IS_INSTANCE(instanceValue)) {
        runtimeError("Can only get property of instance or array");
        return false;
    }

    ObjInstance* instance = AS_INSTANCE(instanceValue);

    if (!tableGet(&instance->fields, propertyName, value)) {
        // Potentially accessing a method
        if (!bindMethod(instance->klass, propertyName, instanceValue, value)) {
            char* instanceString = valueToString(instanceValue);
            char* propertyNameString = valueToString(OBJ_VAL(propertyName));
            runtimeError("Instance %s does not have property / method %s",
                instanceString, propertyNameString);
            free(instanceString);
            free(propertyNameString);
            return false;
        }
    }
    return true;
}

static void defineMethod(ObjString* name) {
    Value method = peek(0);
    ObjClass* klass = AS_CLASS(peek(1));

    if (name == vm.initString) {
        klass->initializer = method;
    } else {
        tableSet(&klass->methods, name, method);
    }
    pop();
}

InterpretResult run() {
#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define IS_ADDABLE(value) (IS_STRING(value) || IS_NUMBER(value))
#define BINARY_OP(valueType ,op) \
    do { \
        Value b = pop(); \
        Value a = pop(); \
        if(!IS_NUMBER(a) || !IS_NUMBER(b)) { \
            runtimeError("Operands must be numbers"); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        push(valueType(AS_NUMBER(a) op AS_NUMBER(b))); \
    } while (false) \

    for (;;) {
        CallFrame* frame = &vm.frames[vm.frameCount - 1];
    #ifdef DEBUG_TRACE_EXECUTION
        printf("      ");
        for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
            printf("[ ");
            printValue(*slot);
            printf(" ]");
        }
        printf("\n");
        disassembleInstruction(&frame->closure->function->chunk,
        (int)(frame->ip - frame->closure->function->chunk.code));
    #endif
        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_RETURN: {
                if (vm.frameCount == 1) return INTERPRET_OK;
                Value value = pop();
                closeUpvalue(frame->slots);
                popFrame();
                push(value);
                break;
            }
            case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
            case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
            case OP_DIVIDE: BINARY_OP(NUMBER_VAL, /); break;
            case OP_LESS: BINARY_OP(BOOL_VAL, <); break;
            case OP_GREATER: BINARY_OP(BOOL_VAL, >); break;
            case OP_TRUE: push(BOOL_VAL(true)); break;
            case OP_FALSE: push(BOOL_VAL(false)); break;
            case OP_NIL: push(NIL_VAL); break;
            case OP_NOT: push(BOOL_VAL(isFalsey(pop()))); break;

            case OP_ADD: {
                if (!IS_ADDABLE(peek(0)) || !IS_ADDABLE(peek(1))) {
                    runtimeError("Can only add strings or numbers");
                }


                if (IS_STRING(peek(0)) || IS_STRING(peek(1))) {
                    concatenate();
                    break;
                }

                BINARY_OP(NUMBER_VAL, +);
                break;
            }

            case OP_NEGATE: {
                if (!IS_NUMBER(peek(0))) {
                    runtimeError("Operand must be number");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(NUMBER_VAL(-AS_NUMBER(pop())));
                break;
            }

            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(constant);
                break;
            }

            case OP_DEFINE_GLOBAL: {
                ObjString* identifier = READ_STRING();
                tableSet(&vm.globals, identifier, peek(0));
                pop();
                break;
            }

            case OP_GET_GLOBAL: {
                ObjString* identifier = READ_STRING();
                Value value;
                bool globalExists = tableGet(&vm.globals, identifier, &value);
                if (globalExists) {
                    push(value);
                } else {
                    runtimeError("Undefined variable '%s'", identifier->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            case OP_SET_GLOBAL : {
                // Must already be defined
                ObjString* identifier = READ_STRING();
                if (!tableGet(&vm.globals, identifier, NULL)) {
                    runtimeError("Undefined variable '%s'", identifier->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                tableSet(&vm.globals, identifier, peek(0)); // Left on stack
                break;
            }

            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push(frame->slots[slot]);
                break;
            }

            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(0);
                break;
            }

            case OP_EQUAL: {
                Value a = pop();
                Value b = pop();
                push(BOOL_VAL(valuesEqual(a, b)));
                break;
            }

            case OP_PRINT: {
                printValue(pop());
                printf("\n");
                break;
            }

            case OP_POP: {
                pop();
                break;
            }

            case OP_POP_COUNT: {
                popCount(READ_BYTE());
                break;
            }


            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (isFalsey(peek(0))) frame->ip += offset;
                break;
            }

            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }

            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }

            case OP_CALL: {
                uint8_t argumentCount = READ_BYTE();
                Value value = peek(argumentCount);
                if (!IS_OBJ(value)) {
                    runtimeError("Can only call functions");
                    return INTERPRET_RUNTIME_ERROR;
                }
                Obj* callable = AS_OBJ(value);
                switch (callable->type) {
                    case OBJ_CLOSURE: {
                        ObjClosure* closure = (ObjClosure*) callable;
                        if (!addFrame(closure, argumentCount)) return INTERPRET_RUNTIME_ERROR;
                        break;
                    }

                    case OBJ_CLASS: {
                        ObjClass* klass = (ObjClass*) callable;
                        ObjInstance* instance = newInstance(klass);
                        Value instanceVal = OBJ_VAL(instance);

                        // Check for initializer:
                        if (IS_NIL(klass->initializer)) {
                            if (argumentCount != 0) {
                              runtimeError("No initializer has been declared for this class");
                              return INTERPRET_RUNTIME_ERROR;
                            }
                            pop(); // Remove class
                            push(instanceVal);
                            break;

                        }

                        ObjClosure* initializer = AS_CLOSURE(klass->initializer);
                        vm.stackTop[-argumentCount - 1] = instanceVal;
                        bool callSuccess = addFrame(initializer, argumentCount);
                        if (!callSuccess) return INTERPRET_RUNTIME_ERROR;
                        break;
                    }

                    case OBJ_BOUND_METHOD: {
                        ObjBoundMethod* boundMethod = (ObjBoundMethod*) callable;
                        vm.stackTop[-argumentCount - 1] = boundMethod->receiver;
                        bool callSuccess = addFrame(boundMethod->method, argumentCount);
                        if (!callSuccess) return INTERPRET_RUNTIME_ERROR;
                        break;
                    }

                    default: {
                        runtimeError("Can only call functions, methods or classes");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                }
                break;
            }

            case OP_CREATE_ARRAY: {
                uint8_t count = READ_BYTE();
                vm.stackTop -= count;
                ObjArray* array = newArray(vm.stackTop, count);
                push(OBJ_VAL(array));
                break;
            }

            case OP_GET_ARRAY: {
                Value indexValue = pop();

                if (IS_STRING(indexValue)) {
                    Value instanceValue = pop();
                    ObjString* propertyName = AS_STRING(indexValue);
                    Value value;
                    if (!getProperty(instanceValue, propertyName, &value)) {
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    push(value);
                    break;
                }

                if (!IS_NUMBER(indexValue)) {
                    runtimeError("Index must be a number");
                    return INTERPRET_RUNTIME_ERROR;
                }
                Value arrayValue = pop();
                if (!IS_ARRAY(arrayValue)) {
                    runtimeError("Can only index into arrays");
                    return INTERPRET_RUNTIME_ERROR;
                }
                int index = AS_NUMBER(indexValue);
                ObjArray* array = AS_ARRAY(arrayValue);
                if (index >= array->valueArray.count) {
                    runtimeError("Provided index is out of bounds");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(array->valueArray.values[index]);
                break;
            }

            case OP_SET_ARRAY: {
                Value newValue = pop();
                Value indexValue = pop();

                if (IS_STRING(indexValue)) {
                    // Field access
                    Value instanceValue = pop();
                    ObjString* propertyName = AS_STRING(indexValue);
                    if (!setProperty(instanceValue, propertyName, newValue)) {
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    push(newValue);
                    break;
                }

                if (!IS_NUMBER(indexValue)) {
                    runtimeError("Index must be a number");
                    return INTERPRET_RUNTIME_ERROR;
                }
                Value arrayValue = pop();
                if (!IS_ARRAY(arrayValue)) {
                    runtimeError("Can only index into arrays");
                    return INTERPRET_RUNTIME_ERROR;
                }
                int index = AS_NUMBER(indexValue);
                ObjArray* array = AS_ARRAY(arrayValue);
                array->valueArray.values[index] = newValue;
                push(newValue);
                break;
            }

            case OP_DUPLICATE: {
                uint8_t offset = READ_BYTE();
                push(peek(offset));
                break;
            }

            case OP_APPEND: {
                Value value = pop();
                Value arrayValue = peek(0);
                if (!IS_ARRAY(arrayValue)) {
                    runtimeError("Can only append to arrays");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjArray* array = AS_ARRAY(arrayValue);
                writeValueArray(&array->valueArray, value);
                break;
            }

            case OP_CLOSURE: {
                ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
                push(OBJ_VAL(function));
                ObjClosure* closure = newClosure(function);
                pop();
                push(OBJ_VAL(closure));
                for (int i = 0; i < closure->upvalueCount; i++) {
                    bool isLocal = READ_BYTE() == 1;
                    uint8_t index = READ_BYTE();
                    ObjUpvalue* upvalue;
                    if (isLocal) {
                        upvalue = captureUpvalue(frame->slots + index);
                    } else {
                        upvalue = frame->closure->upvalues[index];
                    }
                    closure->upvalues[i] = upvalue;
                }
                break;
            }

            case OP_GET_UPVALUE: {
                uint8_t index = READ_BYTE();
                push(*frame->closure->upvalues[index]->location);
                break;
            }

            case OP_SET_UPVALUE: {
                uint8_t index = READ_BYTE();
                Value value = peek(0);
                *frame->closure->upvalues[index]->location = value;
                break;
            }

            case OP_CLOSE_UPVALUE: {
                closeUpvalue(vm.stackTop - 1);
                pop();
                break;
            }

            case OP_CLASS: {
                ObjClass* klass = newClass(READ_STRING());
                push(OBJ_VAL(klass));
                break;
            }

            case OP_GET_PROPERTY: {
                ObjString* propertyName = READ_STRING();
                Value instanceValue = pop();
                Value value;
                if (!getProperty(instanceValue, propertyName, &value)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(value);
                break;
            }

            case OP_SET_PROPERTY: {
                ObjString* propertyName = READ_STRING();
                Value value = pop();
                Value instanceValue = pop();
                if (!setProperty(instanceValue, propertyName, value)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(value);
                break;
            }

            case OP_METHOD: {
                defineMethod(READ_STRING());
                break;
            }

            case OP_INVOKE: {
                ObjString* methodName = READ_STRING();
                uint8_t argumentCount = READ_BYTE();

                ObjInstance* instance = AS_INSTANCE(peek(argumentCount));
                Value methodValue;
                if (!tableGet(&instance->klass->methods, methodName, &methodValue)) {
                    // Check if callable attribute exists
                    if (!tableGet(&instance->fields, methodName, &methodValue)) {
                        runtimeError("Method / function field does not exist");
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    vm.stackTop[-argumentCount - 1] = methodValue;
                } else {
                    vm.stackTop[-argumentCount - 1] = OBJ_VAL(instance);
                }

                ObjClosure* closure = AS_CLOSURE(methodValue);
                if (!addFrame(closure, argumentCount)) return INTERPRET_RUNTIME_ERROR;

                break;
            }

            case OP_INHERIT: {
                Value superclassValue = peek(1);
                if (!IS_CLASS(superclassValue)) {
                    runtimeError("Can only inherit from another class");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjClass* superclass = AS_CLASS(superclassValue);
                ObjClass* subclass = AS_CLASS(peek(0));

                tableAddAll(&superclass->methods, &subclass->methods);
                pop();
                break;
            }

            case OP_GET_SUPER: {
                // [this][super]
                Value instanceValue = peek(1);
                ObjClass* superclass = AS_CLASS(peek(0));
                ObjString* methodName = READ_STRING();
                Value methodValue;
                if (!tableGet(&superclass->methods, methodName, &methodValue)) {
                    runtimeError("Superclass does not have method: %s", methodName);
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjBoundMethod* boundMethod = newBoundMethod(instanceValue, AS_CLOSURE(methodValue));
                popCount(2);
                push(OBJ_VAL(boundMethod));
                break;
            }

            default: {
                runtimeError("Unrecognized instruction");
                return INTERPRET_RUNTIME_ERROR;
            }
        }
    }
#undef READ_CONSTANT
#undef READ_BYTE
#undef READ_SHORT
#undef READ_STRING
#undef BINARY_OP
}

InterpretResult interpret(const char* source) {
    // Wrap function in a closure:
    ObjFunction* function = compile(source);
    push(OBJ_VAL((Obj*)function));
    if (function == NULL) return INTERPRET_COMPILE_ERROR;
    ObjClosure* closure = newClosure(function);
    pop();

    // Put the 'main' callable on the stack

    push(OBJ_VAL(closure));
    addFrame(closure, 0);

    return run();
}

void printObjects() {
    // Needs to be fixed
    Obj* ptr = vm.objects;
    while (ptr != NULL) {
        ObjString* string = (ObjString*) ptr;
        printf("%s -> ", string->chars);
        ptr = ptr->next;
    }
    printf("NULL");
}


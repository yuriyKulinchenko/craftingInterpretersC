#include "vm.h"
#include "common.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "debug.h"
#include "compiler.h"
#include "memory.h"
#include "object.h"

VM vm;

void resetStack() {
    vm.stackTop = vm.stack;
}

static void runtimeError(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n"
    , stderr);

    size_t instruction = vm.ip - vm.chunk->code - 1;
    int line = vm.chunk->lines[instruction];
    fprintf(stderr, "[line %d] in script\n", line);

    resetStack();
}

void initVM() {
    resetStack();
    vm.objects = NULL;
    initTable(&vm.strings);
}

void freeVM() {
    freeObjects();
    freeTable(&vm.strings);
}

void push(Value value) {
    *vm.stackTop = value;
    vm.stackTop++;
}

Value pop() {
    vm.stackTop--;
    return *vm.stackTop;
}

Value peek(int n) {
    return vm.stackTop[-1 - n];
}

static bool isFalsey(Value value) {
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static bool objEqual(Obj* aPtr, Obj* bPtr) {
    if (aPtr->type != bPtr->type) return false;

    switch (aPtr->type) {
        case OBJ_STRING: {
            return aPtr == bPtr; // Reference equality now works because of interning
        }
        default: return false;
    }
}

static bool valuesEqual(Value a, Value b) {
    if (a.type != b.type) return false;

    switch (a.type) {
        case VAL_NIL: return true;
        case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_BOOL: return AS_BOOL(a) == AS_BOOL(b);
        case VAL_OBJ: return objEqual(AS_OBJ(a), AS_OBJ(b));
        default: return false;
    }
}

void concatenate() {
    ObjString* bString = AS_STRING(pop());
    ObjString* aString = AS_STRING(pop());
    // Dynamically allocate new object
    int length = aString->length + bString->length;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, aString->chars, aString->length);
    memcpy(chars + aString->length, bString->chars, bString->length);
    chars[length] = '\0';
    ObjString* result = takeString(chars, length);
    push(OBJ_VAL(result));
}

InterpretResult run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define BINARY_OP(valueType ,op) \
    do { \
        Value b = pop(); \
        Value a = pop(); \
        if(!IS_NUMBER(a) || !IS_NUMBER(b)) { \
            runtimeError("operands must be numbers"); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        push(valueType(AS_NUMBER(a) op AS_NUMBER(b))); \
    } while (false) \

    for (;;) {
    #ifdef DEBUG_TRACE_EXECUTION
        printf("      ");
        for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
            printf("[ ");
            printValue(*slot);
            printf(" ]");
        }
        printf("\n");
        disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
    #endif
        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_RETURN: {
                return INTERPRET_OK;
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
                if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                    concatenate();
                    break;
                }
                BINARY_OP(NUMBER_VAL, +);
                break;
            }

            case OP_NEGATE: {
                if (!IS_NUMBER(peek(0))) {
                    runtimeError("Operand must be number");
                }
                push(NUMBER_VAL(-AS_NUMBER(pop())));
                break;
            }

            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(constant);
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
            }

        }
    }
#undef READ_CONSTANT
#undef READ_BYTE
#undef BINARY_OP
}

InterpretResult interpret(const char* source) {
    Chunk chunk;
    initChunk(&chunk);

    if (!compile(source, &chunk)) {
        freeChunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;

    InterpretResult result = run();

    freeChunk(&chunk);
    return result;
}

void printObjects() {
    Obj* ptr = vm.objects;
    while (ptr != NULL) {
        ObjString* string = (ObjString*) ptr;
        printf("%s -> ", string->chars);
        ptr = ptr->next;
    }
    printf("NULL");
}


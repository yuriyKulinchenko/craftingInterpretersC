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
    initTable(&vm.globals);
    initTable(&vm.constants);
}

void freeVM() {
    freeObjects();
    freeTable(&vm.strings);
    freeTable(&vm.globals);
    freeTable(&vm.constants);
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
#define READ_SHORT() (vm.ip += 2, (uint16_t)((vm.ip[-2] << 8) | vm.ip[-1]))
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())
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
                push(vm.stack[slot]);
                break;
            }

            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                vm.stack[slot] = peek(0);
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
                if (isFalsey(peek(0))) vm.ip += offset;
                break;
            }

            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                vm.ip += offset;
                break;
            }

            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                vm.ip -= offset;
                break;
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
    Chunk chunk;
    initChunk(&chunk);

    if (!compile(source, &chunk)) {
        freeChunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    resetStack();

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


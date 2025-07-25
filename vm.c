#include "vm.h"
#include "common.h"
#include <stdio.h>
#include "debug.h"
#include "compiler.h"

VM vm;

void resetStack() {
    vm.stackTop = vm.stack;
}

void initVM() {
    resetStack();
}

void freeVM() {

}

void push(Value value) {
    *vm.stackTop = value;
    vm.stackTop++;
}

Value pop() {
    vm.stackTop--;
    return *vm.stackTop;
}

InterpretResult run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define BINARY_OP(op) \
    do { \
        double b = pop(); \
        double a = pop(); \
        push(a op b); \
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
                printf("      ");
                printValue(pop());
                printf("\n");
                return INTERPRET_OK;
            }
            case OP_ADD: BINARY_OP(+); break;
            case OP_SUBTRACT: BINARY_OP(-); break;
            case OP_MULTIPLY: BINARY_OP(*); break;
            case OP_DIVIDE: BINARY_OP(/); break;
            case OP_NEGATE: push(-pop()); break;
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(constant);
                break;
            }
        }
    }
#undef READ_CONSTANT
#undef READ_BYTE
#undef BINARY_OP
}

InterpretResult interpret(const char* source) {
    compile(source);
    return INTERPRET_OK;
}

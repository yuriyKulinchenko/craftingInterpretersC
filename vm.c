#include "vm.h"
#include "common.h"
#include <stdio.h>
#include "debug.h"

VM vm;

void initVM() {

}

void freeVM() {

}

InterpretResult run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
    for (;;) {
    #ifdef DEBUG_TRACE_EXECUTION
        disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
    #endif
        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_RETURN: {
                return INTERPRET_OK;
            }
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                printValue(constant);
                printf("\n");
                break;
            }
        }
    }
#undef READ_CONSTANT
#undef READ_BYTE
}

InterpretResult interpret(Chunk* chunk) {
    vm.chunk = chunk;
    vm.ip = vm.chunk->code;
    return run();
}

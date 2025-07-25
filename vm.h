//
// Created by Yuriy Kulinchenko on 24/07/2025.
//

#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "value.h"

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
}InterpretResult;

#define STACK_MAX 256

typedef struct {
    Chunk* chunk;
    uint8_t* ip;
    Value stack[STACK_MAX];
    Value* stackTop;
} VM;

void initVM();
void freeVM();
InterpretResult interpret(const char* source);
void push();
Value pop();

#endif

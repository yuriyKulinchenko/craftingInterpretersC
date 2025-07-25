#include "chunk.h"
#include "common.h"
#include "stdio.h"
#include "debug.h"
#include "vm.h"

int main(int argc, const char* argv[]) {
    initVM();
    Chunk chunk;
    initChunk(&chunk);

    uint8_t c1 = addConstant(&chunk, 1.2);
    uint8_t c2 = addConstant(&chunk, 3.4);
    uint8_t c3 = addConstant(&chunk, 5.6);

    writeChunk(&chunk, OP_CONSTANT, 123);
    writeChunk(&chunk, c1, 123);

    writeChunk(&chunk, OP_CONSTANT, 123);
    writeChunk(&chunk, c2, 123);

    writeChunk(&chunk, OP_ADD, 123);

    writeChunk(&chunk, OP_CONSTANT, 123);
    writeChunk(&chunk, c3, 123);

    writeChunk(&chunk, OP_DIVIDE, 123);

    writeChunk(&chunk, OP_NEGATE, 123);

    writeChunk(&chunk, OP_RETURN, 123);


    //disassembleChunk(&chunk, "test chunk");
    interpret(&chunk);
    freeVM();
    freeChunk(&chunk);
    return 0;
}

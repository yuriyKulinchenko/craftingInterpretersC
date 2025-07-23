#include "chunk.h"
#include "common.h"
#include "stdio.h"

int main(int argc, const char* argv[]) {
    Chunk chunk;
    initChunk(&chunk);
    writeChunk(&chunk, OP_RETURN);
    freeChunk(&chunk);
    printf("done!\n");
    return 0;
}

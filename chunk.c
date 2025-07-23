#include "chunk.h"

#include <stdlib.h>

void initChunk(Chunk* chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
}

void* reallocate(void* p, size_t oldSize, size_t newSize) {
    if (newSize == 0) {
        free(p);
        return NULL;
    }
    void* result = realloc(p, newSize);
    if (result == NULL) exit(1);
    return result;
}

void writeChunk(Chunk* chunk, uint8_t byte) {
    if (chunk->capacity < chunk->count + 1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = oldCapacity < 8 ? 8 : oldCapacity * 2;
        chunk->code = reallocate(chunk->code, oldCapacity, chunk->capacity);
    }
    chunk->code[chunk->capacity++] = byte;
}

void freeChunk(Chunk* chunk) {
    reallocate(chunk->code, chunk->capacity, 0);
}
//
// Created by Yuriy Kulinchenko on 23/07/2025.
//

#ifndef clox_memory_h
#define clox_memory_h

#include "common.h"

#define ALLOCATE(type, count) ((type*)reallocate(NULL, 0, sizeof(type) * (count)))

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
(type*)reallocate(pointer, (oldCount) * sizeof(type), (newCount) * sizeof(type))

#define FREE_ARRAY(type, pointer, oldCount) GROW_ARRAY(type, pointer, oldCount, 0)

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

void collectGarbage();
void* reallocate(void* p, size_t oldSize, size_t newSize);
void freeObjects();

#endif

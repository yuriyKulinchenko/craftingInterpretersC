//
// Created by Yuriy Kulinchenko on 23/07/2025.
//

#ifndef clox_memory_h
#define clox_memory_h

#include "common.h"

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
(type*)reallocate(pointer, (oldCount) * sizeof(type), (newCount) * sizeof(type))

void* reallocate(void* p, size_t oldSize, size_t newSize);

#endif

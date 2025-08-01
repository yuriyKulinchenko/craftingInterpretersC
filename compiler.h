//
// Created by Yuriy Kulinchenko on 25/07/2025.
//

#ifndef clox_compiler_h
#define clox_compiler_h

#include "vm.h"
#include "object.h"


bool compile(const char* source, Chunk* chunk);

#endif

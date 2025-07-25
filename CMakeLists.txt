cmake_minimum_required(VERSION 3.31)
project(craftingInterpretersC C)

set(CMAKE_C_STANDARD 11)

add_executable(craftingInterpretersC main.c
        common.h
        chunk.h
        chunk.c
        debug.h
        debug.c
        value.h
        value.c
        memory.h
        memory.c
        vm.h
        vm.c
        compiler.h
        compiler.c
        scanner.h
        scanner.c)

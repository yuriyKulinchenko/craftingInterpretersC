cmake_minimum_required(VERSION 3.31)
project(craftingInterpretersC C)

set(CMAKE_C_STANDARD 11)

add_executable(craftingInterpretersC main.c
        common.h
        chunk.h
        chunk.c)

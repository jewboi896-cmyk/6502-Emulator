cmake_minimum_required(VERSION 4.2)
project(6502Emulator C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(6502Emulator 6502-emulator-main.c
        6502emulator.h
        6502emulator.h)

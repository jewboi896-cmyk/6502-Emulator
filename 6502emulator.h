//
// Created by derek on 8/24/2026.
//

// note that #pragma once is not a part of the official C ISO standard
#pragma once

#include <stdint.h>

extern uint8_t sys_mem[65536];
extern uint64_t sys_clock_cycles;

// Architecture Types
union StatusRegister
{
    uint8_t reg;
    struct {
        uint8_t carry : 1;
        uint8_t zero : 1;
        uint8_t id : 1;
        uint8_t dm : 1;
        uint8_t break_cmd : 1;
        uint8_t unused : 1;
        uint8_t overflow : 1;
        uint8_t negative : 1;
    };
};

struct CPURegister
{
    uint16_t program_counter;
    uint8_t stack_pointer;
    uint8_t accumulator;
    uint8_t x_index;
    uint8_t y_index;
    union StatusRegister status_register;
};

// Core Execution Prototypes
void reset(struct CPURegister* cpu_register);
void step(struct CPURegister* cpu_register);
uint8_t fetch_byte(struct CPURegister* cpu_register);

// Complex Addressing Mode Prototypes
uint16_t get_abs_x_address(struct CPURegister* cpu_register, int* pg_crossed);
uint16_t get_abs_y_address(struct CPURegister* cpu_register, int* pg_crossed);
uint16_t get_indirect_x_address(struct CPURegister* cpu_register);
uint16_t get_indirect_y_address(struct CPURegister* cpu_register, int* pg_crossed);


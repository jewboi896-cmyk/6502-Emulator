//
// Created by derek on 8/19/2026.
//

// simple helper functions are explicitly marked as inline even though the C compiler
// reserves the right to ignore it or inline functions not marked as inline
// IDEs like CLion will mark it as redundant because of this reason

#include <stdint.h>
#include <stdio.h>
#include "6502emulator.h"

// total sys mem 64KB total - typically 32KB of ROM and 16KB of RAM
uint8_t sys_mem[65536];

// track clock cycles
uint64_t sys_clock_cycles = 0;

// forward decs
static inline uint8_t read_bytes(uint16_t address);
static inline void write_bytes(uint16_t address, uint8_t value);
static inline uint16_t get_reset_vector(void);
static inline void tick(void);
static inline uint16_t get_abs_address(struct CPURegister* cpu_register);
static inline uint16_t get_zpg_address(struct CPURegister* cpu_register);
static inline uint16_t get_zpg_x_address(struct CPURegister* cpu_register);
static inline uint16_t get_zpg_y_address(struct CPURegister* cpu_register);
void exec_brnch_if(struct CPURegister* cpu_register, int condit);
static inline void update_nz_flags(struct CPURegister* cpu_register, uint8_t val);

int main()
{
    struct CPURegister cpu_register;

    printf("==================================================\n");
    printf("   Initializing Virtual MOS 6502 Engine Frame...  \n");
    printf("==================================================\n\n");

    // Point hardwired Reset Vector to test program target at $8000
    write_bytes(0xFFFC, 0x00);
    write_bytes(0xFFFD, 0x80);

    // Assemble test binary sequence directly into RAM array starting at $8000
    uint16_t program_ptr = 0x8000;

    // LDA #$42 (Load literal byte value $42 into Accumulator) -> Opcode A9 42
    write_bytes(program_ptr++, 0xA9); write_bytes(program_ptr++, 0x42);

    // ASL A (Arithmetic Shift Left the Accumulator -> makes it $84) -> Opcode 0A
    write_bytes(program_ptr++, 0x0A);

    // STA $0015 (Store Accumulator to Zero-Page memory address $15) -> Opcode 85 15
    write_bytes(program_ptr++, 0x85); write_bytes(program_ptr++, 0x15);

    // LDX #$05 (Load literal byte value $05 into X Index Register) -> Opcode A2 05
    write_bytes(program_ptr++, 0xA2); write_bytes(program_ptr++, 0x05);

    // BRK (Trigger software break / halt sentinel loop) -> Opcode 00
    write_bytes(program_ptr++, 0x00);

    // Initiate the hardware Reset sequence changes
    reset(&cpu_register);
    printf(" Cold Boot Up Initialized -> Program Counter set to: $%04X\n\n",
        cpu_register.program_counter);

    // Run automated execution loop that prints out live register states
    // loop breaks if it jumps to an unmapped hardware handler address
    // (like the BRK vector defaults)
    uint16_t total_steps = 0;
    while (cpu_register.program_counter >= 0x8000
        && cpu_register.program_counter < 0x8010)
    {
        total_steps++;
        printf("[Instruction Step #%d]\n", total_steps);

        step(&cpu_register);

        // print register output
        printf("  CPU Registers -> A: $%02X  X: $%02X  Y: $%02X  SP: $%02X  "
               "PC: $%04X\n",
               cpu_register.accumulator, cpu_register.x_index, cpu_register.y_index,
               cpu_register.stack_pointer, cpu_register.program_counter);

        // print status flags
        printf("  Status Flags  -> N: %d  V: %d  Z: %d  C: %d\n",
               cpu_register.status_register.negative,
               cpu_register.status_register.overflow,
               cpu_register.status_register.zero, cpu_register.status_register.carry);

        // print clock cycles
        printf("  System Timeline Clock Cycles: %llu\n\n", sys_clock_cycles);
    }

    // Verify data loop back side effects inside global mem matrix
    printf("==================================================\n");
    printf(" Memory Verification Check -> Address $0015 reads: $%02X\n",
        read_bytes(0x0015));
    printf("==================================================\n");

    return 0;
}

// reads bytes directly from the systems total core memory
// system does not care whether its reading from RAM or ROM
static inline uint8_t read_bytes(uint16_t address)
{
    return sys_mem[address];
}

// writes bytes from the given address to the given value
static inline void write_bytes(uint16_t address, uint8_t value)
{
    sys_mem[address] = value;
}

// gets the reset vector by combining the high and low bits
static inline uint16_t get_reset_vector(void)
{
    // high bit - 0xFFFD
    // low bit - 0xFFFC
    // pipes the two into the reset vector
    return (read_bytes(0xFFFD) << 8) | read_bytes(0xFFFC);
}

// resets the cpu
void reset(struct CPURegister* cpu_register)
{
    // clear registers to defaults
    cpu_register->accumulator = 0x00;
    cpu_register->x_index = 0x00;
    cpu_register->y_index = 0x00;

    // set status register flag to default
    cpu_register->status_register.reg = 0x24;

    // sim the three dummy hardware stack pushes that happen during a reset
    cpu_register->stack_pointer = 0xFD;

    // reset the stack vector
    cpu_register->program_counter = get_reset_vector();
}

// helper to update the non-zero flags
static inline void update_nz_flags(struct CPURegister* cpu_register, uint8_t val)
{
    cpu_register->status_register.zero = (val == 0);
    cpu_register->status_register.negative = (val & 0x80) ? 1 : 0;
}

// helper for executing conditional branches
void exec_brnch_if(struct CPURegister* cpu_register, int condit)
{
    // not uint8_t because offset cannot be negative and honestly shouldnt be
    int8_t offset = (int8_t)fetch_byte(cpu_register);

    if (condit)
    {
        tick(); // successful exec cycle

        // save old pc in case of page boundary cross
        uint16_t old_pc = cpu_register->program_counter;
        cpu_register->program_counter += offset;

        // check for pg boundary cross
        if ((old_pc & 0xFF00) != (cpu_register->program_counter & 0xFF00))
        {
            tick(); // apply extra cycle penalty for page boundary cross
        }
    }
}

// ticks the system clock cycle up by one
static inline void tick()
{
    sys_clock_cycles++;
}

// steps one opcode at a time
void step(struct CPURegister* cpu_register)
{
    // fetch current opcode
    uint8_t opcode = fetch_byte(cpu_register);

    switch (opcode)
    {
    case 0xA9: // LDA
        {
            // fetch opcode value from register
            cpu_register->accumulator = fetch_byte(cpu_register);

            // update flags
            update_nz_flags(cpu_register, cpu_register->accumulator);
            break;
        }
    case 0xEA: // NOP takes 2 cycles
        {
            // dummy instruction
            tick();
            break;
        }
    case 0x8D: // STA Absolute
        {
            // split bytes
            uint16_t target_address = get_abs_address(cpu_register);

            // write accumulator contents back to target_address
            write_bytes(target_address, cpu_register->accumulator);
            tick(); // internal hardware write cycle
            break;
        }
    case 0xA2: // LDX Immediate
        {
            // fetch value from register
            cpu_register->x_index = fetch_byte(cpu_register);

            // update flags
            update_nz_flags(cpu_register, cpu_register->x_index);
            break;
        }
    case 0x8E: // STX Absolute
        {
            // just like STA get the 16-bit address by splitting it into 2 8 byte
            // addresses starting with the low byte
            uint16_t target_address = get_abs_address(cpu_register);

            // write bytes back to x_index register
            write_bytes(target_address, cpu_register->x_index);
            tick(); // internal hardware write cycle
            // no status flags affected
            break;
        }
    case 0x18: // CLC Implied
        {
            // update the carry bit from union
            cpu_register->status_register.carry = 0;
            tick();
            break;
        }
    case 0xA5: // LDA Zero Pg
        {
            // calc target address using get_zpg_address
            uint16_t target_address = get_zpg_address(cpu_register);

            // read value from zpg mem coord
            cpu_register->accumulator = read_bytes(target_address);

            // set status flags
            update_nz_flags(cpu_register, cpu_register->accumulator);
            break;
        }
    case 0x85: // STA Zero Pg
        {
            // calc dest address
            uint16_t target_address = get_zpg_address(cpu_register);

            // write accumulator back to mem
            write_bytes(target_address, cpu_register->accumulator);
            tick();
            break;
        }
    case 0xB5: // LDA Zero Pg X
        {
            // calc wrapped address
            uint16_t target_address = get_zpg_x_address(cpu_register);

            // sim tick
            tick();

            // read data from wrapped address
            cpu_register->accumulator = read_bytes(target_address);
            tick(); // for mem read bus cycle

            // update flags
            update_nz_flags(cpu_register, cpu_register->accumulator);
            break;
        }
    case 0x96: // STX Zero Pg Y
        {
            // calc wrapped address
            uint16_t target_address = get_zpg_y_address(cpu_register);

            // sim tick
            tick();

            // write X to zero pg target dest
            write_bytes(target_address, cpu_register->x_index);
            tick(); // sim tick
            break;
        }
    case 0xBD: // LDA Absolute X
        {
            // init as false
            int page_crossed = 0;

            // calc target address & determine page crossing status
            uint16_t target_address = get_abs_x_address(cpu_register, &page_crossed);

            // read mem byte
            cpu_register->accumulator = read_bytes(target_address);
            tick(); // standard mem read

            // if page boundary crossed during a read, apply the extra cycle penalty
            if (page_crossed)
            {
                tick();
            }

            // update flags
            update_nz_flags(cpu_register, cpu_register->accumulator);
            break;
        }
    case 0x9D: // STA Absolute X
        {
            // init as false
            int page_crossed = 0;

            // calc address
            uint16_t target_address = get_abs_x_address(cpu_register, &page_crossed);

            // STA always hits hardware with a dummy tick on writes
            tick();

            // write data to resolved address
            write_bytes(target_address, cpu_register->accumulator);
            tick();
            break;
        }
    case 0xA1: // Indirect LDA X
        {
            // resolve 16-bit target address
            uint16_t target_address = get_indirect_x_address(cpu_register);

            // sim ticks
            // 2 for the read cycles in the function above
            // 1 for the internal cpu calc cycle
            tick();
            tick();
            tick();

            // read final value from resolved address and issue a clock cycle afterward
            cpu_register->accumulator = read_bytes(target_address);
            tick(); // sync clock

            // update flag statuses
            update_nz_flags(cpu_register, cpu_register->accumulator);
            break;
        }
    case 0xB1: // Indirect LDA Y
        {
            int page_crossed = 0;

            // resolve target address
            uint16_t target_address = get_indirect_y_address(cpu_register, &page_crossed);

            //  add 2 ticks for the two read_bytes cycles
            tick();
            tick();

            // read final target data byte across the bus
            cpu_register->accumulator = read_bytes(target_address);
            tick(); // data read cycle

            // if page boundary crossed, add the 6th cycle penalty
            if (page_crossed)
            {
                tick();
            }

            // update status flags
            update_nz_flags(cpu_register, cpu_register->accumulator);
            break;
        }
    case 0x4C: // JMP Absolute
        {
            // fetch full 16-bit address across the bus
            uint16_t target_address = get_abs_address(cpu_register);

            // overwrite pc to force exec jmp
            cpu_register->program_counter = target_address;
            break;
        }
    // the 6502 has a hardware bug where if a 16-bit location falls exactly on a page
    // boundary, the cpu reads the low byte and tries to cross the page boundary to read
    // the next high byte on the next page. in doing so, the internal adder fails to
    // carry over the high byte in time. it reads the low byte and then loops back to
    // the beginning of the previous page
    case 0x6C: // JMP Indirect
        {
            // fetch 16-bit address vector
            uint8_t low_vec = fetch_byte(cpu_register);
            uint8_t high_vec = fetch_byte(cpu_register);
            uint16_t vec = (high_vec << 8) | low_vec;

            uint16_t target_address;

            // emulate the page bug described above
            if ((vec & 0x00FF) == 0x00FF)
            {
                // bugged state: low byte at $XXFF, high byte at $XX00 will wrap around
                uint16_t low_byte_address = vec;
                uint16_t high_byte_address = vec & 0xFF00; // forces low byte to 0x00

                uint8_t target_low = read_bytes(low_byte_address);
                uint8_t target_high = read_bytes(high_byte_address);
                target_address = (target_high << 8) | target_low;
            } else
            {
                // normal operations: vecs are sequential
                uint8_t target_low = read_bytes(vec);
                uint8_t target_high = read_bytes(vec + 1);
                target_address = (target_high << 8) | target_low;
            }

            // sim 2 ticks for the two read_bytes ops
            tick();
            tick();

            // update pc with final address from mem
            cpu_register->program_counter = target_address;
            break;
        }
    case 0x20: // JSR Absolute
        {
            // fetch 16-bit target dest
            uint8_t target_low = fetch_byte(cpu_register);
            uint8_t target_high = fetch_byte(cpu_register);
            uint16_t target_address = (uint16_t)((target_high << 8) | target_low);

            // internal stack hardware delay tick
            tick();

            // push high byte into page 1 stack mem
            uint16_t ret_address = cpu_register->program_counter - 1;

            write_bytes((uint16_t)(0x0100 + cpu_register->stack_pointer),
                (uint8_t)(ret_address >> 8) & 0xFF);
            cpu_register->stack_pointer--;
            tick(); // high byte stack write bus cycle

            write_bytes((uint16_t)(0x0100 + cpu_register->stack_pointer),
                (uint8_t)(ret_address & 0xFF));
            cpu_register->stack_pointer--;
            tick(); // high byte stack write bus cycle

            // update pc counter
            cpu_register->program_counter = target_address;
            break;
        }
    case 0x60: // RTS Implied
        {
            tick(); // internal stack layout decode

            // pull low byte off stack
            cpu_register->stack_pointer++;
            uint8_t low_byte = read_bytes((uint16_t)(0x0100 + cpu_register->stack_pointer));
            tick(); // stack read low byte cycle

            // pull high byte off stack
            cpu_register->stack_pointer++;
            uint8_t high_byte = read_bytes((uint16_t)(0x0100 + cpu_register->stack_pointer));
            tick(); // stack read high byte cycle

            // combine into ret address and apply 5th cycle adjustment
            uint16_t ret_address = (uint16_t)(high_byte << 8) | low_byte;
            tick(); // internal routing tick

            // restore exec path by 1 per 6502 spec
            cpu_register->program_counter = (uint16_t)(ret_address + 1);
            break;
        }
    case 0xD0: // BNE
        {
            exec_brnch_if(cpu_register, cpu_register->status_register.zero == 0);
            break; // if conditional fails, simply skip if-block
        }
    case 0xF0: // BEQ
        {
            exec_brnch_if(cpu_register, cpu_register->status_register.zero == 1);
            break;
        }
    case 0x90: // BCC
        {
            exec_brnch_if(cpu_register, cpu_register->status_register.carry == 0);
            break;
        }
    case 0xB0: // BCS
        {
            exec_brnch_if(cpu_register, cpu_register->status_register.carry == 1);
            break;
        }
    case 0x10: // BPL
        {
            exec_brnch_if(cpu_register, cpu_register->status_register.negative == 0);
            break;
        }
    case 0x30: // BMI
        {
            exec_brnch_if(cpu_register, cpu_register->status_register.negative == 1);
            break;
        }
    case 0x50: // BVC
        {
            exec_brnch_if(cpu_register, cpu_register->status_register.overflow == 0);
            break;
        }
    case 0x70: // BVS
        {
            exec_brnch_if(cpu_register, cpu_register->status_register.overflow == 1);
            break;
        }
    case 0x69: // ADC Immediate
        {
            uint8_t operand = fetch_byte(cpu_register);

            // calc result in 16-bit to safely capture carry bit
            uint16_t sum = (uint16_t)cpu_register->accumulator + (uint16_t)operand
            + (uint16_t)cpu_register->status_register.carry;

            // eval signed overflow before collapsing into 8-bit
            cpu_register->status_register.overflow = (!((cpu_register->accumulator
                ^ operand) & 0x80) && ((cpu_register->accumulator ^ sum) & 0x80))
            ? 1 : 0;

            // store lower 8-bits back in accumulator
            cpu_register->accumulator = (uint8_t)(sum & 0xFF);

            // capture unsigned overflow: 1 if 16-bit sum is active
            cpu_register->status_register.carry = (sum > 0xFF) ? 1 : 0;

            // update status flags
            update_nz_flags(cpu_register, cpu_register->accumulator);
            break;
        }
    case 0xE9: // SBC Immediate
        {
            uint8_t operand = fetch_byte(cpu_register);

            // invert to perform subtraction
            uint8_t inverted_op = ~operand;

            // perform subtraction
            uint16_t diff = cpu_register->accumulator + (uint16_t)inverted_op + (uint16_t)cpu_register->status_register.carry;

            // signed overflow using inverted val
            cpu_register->status_register.overflow = (!((cpu_register->accumulator
                ^ inverted_op) & 0x80) && ((cpu_register->accumulator ^ diff) & 0x80))
            ? 1 : 0;

            // carry is set if 16-bit sum exceeds 0xFF
            cpu_register->status_register.carry = (diff > 0xFF) ? 1 : 0;

            // update status flags
            update_nz_flags(cpu_register, cpu_register->accumulator);
            break;
        }
    case 0x29: // AND Immediate
        {
            uint8_t operand = fetch_byte(cpu_register);

            // perform bitwise AND
            cpu_register->accumulator &= operand;

            // update status flags
            update_nz_flags(cpu_register, cpu_register->accumulator);
            break;
        }
    case 0x09: // ORA Immediate
        {
            uint8_t operand = fetch_byte(cpu_register);

            // perform bitwise OR
            cpu_register->accumulator |= operand;

            // update flags
            update_nz_flags(cpu_register, cpu_register->accumulator);
            break;
        }
    case 0x49: // EOR Immediate
        {
            uint8_t operand = fetch_byte(cpu_register);

            // perform bitwise xor
            cpu_register->accumulator ^= operand;

            // update flags
            update_nz_flags(cpu_register, cpu_register->accumulator);
            break;
        }
    case 0x2C: // BIT Absolute
        {
            uint16_t target_address = get_abs_address(cpu_register);
            uint8_t val = read_bytes(target_address);

            tick(); // mem read cycle

            // set zero flag if AND val == 0
            cpu_register->status_register.zero = ((cpu_register->accumulator & val) == 0);

            // transfer bit 7 of mem val directly to negative flag
            cpu_register->status_register.negative = (val & 0x80) ? 1 : 0;

            // transfer bit 6 the same way
            cpu_register->status_register.overflow = (val & 0x40) ? 1 : 0;
            break;
        }
    case 0x0A: // ASL Accumulator
        {
            // shift bit 7 into carry flag
            cpu_register->status_register.carry = (cpu_register->accumulator & 0x80) ? 1 : 0;

            // perform the bit shift
            cpu_register->accumulator <<= 1;

            // update status flags
            update_nz_flags(cpu_register, cpu_register->accumulator);
            break;
        }
    case 0x0E: // ASL Absolute
        {
            uint16_t target_address = get_abs_address(cpu_register);
            uint8_t val = read_bytes(target_address);

            tick(); // mem read
            tick(); // internal shift step

            cpu_register->status_register.carry = (val & 0x80) ? 1 : 0;
            val <<= 1;

            write_bytes(target_address, val);
            tick(); // mem write back

            // update flags
            update_nz_flags(cpu_register, val);
            break;
        }
    case 0x4A: // LSR Accumulator
        {
            // shift bit 0 into carry flag
            cpu_register->status_register.carry = (cpu_register->accumulator & 0x01);

            // perform logical right shift
            cpu_register->accumulator >>= 1;

            // update status flags
            update_nz_flags(cpu_register, cpu_register->accumulator);
            break;
        }
    case 0x4E: // LSR Absolute
        {
            uint16_t target_address = get_abs_address(cpu_register);
            uint8_t val = read_bytes(target_address);

            tick(); // read cycle
            tick(); // internal shift cycle

            cpu_register->status_register.carry = (val & 0x01);
            val >>= 1;

            write_bytes(target_address, val);
            tick(); // write back cycle

            // update flags
            update_nz_flags(cpu_register, val);
            break;
        }
    case 0x2A: // ROL Accumulator
        {
            uint8_t old_carry = cpu_register->status_register.carry;

            // move bit 7 to carry
            cpu_register->status_register.carry = (cpu_register->accumulator & 0x80)
            ? 1 : 0;

            // shift left and inject old carry into bit 0
            cpu_register->accumulator = (cpu_register->accumulator << 1) | old_carry;

            // update status flags
            update_nz_flags(cpu_register, cpu_register->accumulator);
            break;
        }
    case 0x2E: // ROL Absolute
        {
            uint16_t target_address = get_abs_address(cpu_register);
            uint8_t val = read_bytes(target_address);

            tick(); // read cycle
            tick(); // internal shift cycle

            uint8_t old_carry = cpu_register->status_register.carry;
            cpu_register->status_register.carry = (val & 0x80) ? 1 : 0;
            val = (val << 1) | (old_carry);

            write_bytes(target_address, val);
            tick(); // write cycle

            // update status flags
            update_nz_flags(cpu_register, val);
            break;
        }
    case 0x6A: // ROR Accumulator
        {
            uint8_t old_carry = cpu_register->status_register.carry;

            // move bit 0 to carry
            cpu_register->status_register.carry = (cpu_register->accumulator & 0x01);

            // shift right and inject old carry into bit 7
            uint8_t result = (cpu_register->accumulator >> 1) | (old_carry << 7);
            cpu_register->accumulator = result;

            // update status flags
            update_nz_flags(cpu_register, cpu_register->accumulator);
            break;
        }
    case 0x6E: // ROR Absolute
        {
            uint16_t target_address = get_abs_address(cpu_register);
            uint8_t val = read_bytes(target_address);

            tick(); // read cycle
            tick(); // internal shift cycle

            uint8_t old_carry = cpu_register->status_register.carry;
            cpu_register->status_register.carry = (val & 0x01);
            val = (val >> 1) | (old_carry << 7);

            write_bytes(target_address, val);
            tick(); // write cycle

            // update flags
            update_nz_flags(cpu_register, val);
            break;
        }
    case 0xE8: // INX Implied
        {
            cpu_register->x_index++;
            tick(); // internal register increment

            // update status flags
            update_nz_flags(cpu_register, cpu_register->x_index);
            break;
        }
    case 0xCA: // DEX Implied
        {
            cpu_register->x_index--;
            tick(); // internal decrement step

            // update flags
            update_nz_flags(cpu_register, cpu_register->x_index);
            break;
        }
    case 0xC8: // INY Implied
        {
            cpu_register->y_index++;
            tick(); // internal increment step

            // update flags
            update_nz_flags(cpu_register, cpu_register->y_index);
            break;
        }
    case 0x88: // DEY Implied
        {
            cpu_register->y_index--;
            tick(); // internal decrement step

            // update flags
            update_nz_flags(cpu_register, cpu_register->y_index);
            break;
        }
    case 0xEE: // INC Absolute
        {
            uint16_t target_address = get_abs_address(cpu_register);
            uint8_t val = read_bytes(target_address);

            tick(); // mem read cycle
            tick(); // internal math cycle

            val++; // increment with 8-bit wrap around

            write_bytes(target_address, val);
            tick(); // mem write cycle

            // update flags
            update_nz_flags(cpu_register, val);
            break;
        }
    case 0xCE: // DEC Absolute
        {
            uint16_t target_address = get_abs_address(cpu_register);
            uint8_t val = read_bytes(target_address);

            tick(); // mem read cycle
            tick(); // internal math cycle

            // decrement with 8-bit wrap-around
            val--;

            write_bytes(target_address, val);
            tick(); // mem write cycle

            // update flags
            update_nz_flags(cpu_register, val);
            break;
        }
    case 0xC9: // CMP Immediate
        {
            uint8_t operand = fetch_byte(cpu_register);

            // set carry if accum >= op
            cpu_register->status_register.carry = (cpu_register->accumulator >= operand)
            ? 1 : 0;

            // calc temp 8-bit res to determine z & n flags
            uint8_t temp_res = cpu_register->accumulator - operand;

            // update flags
            update_nz_flags(cpu_register, temp_res);
            break;
        }
    case 0xE0: // CPX Immediate
        {
            uint8_t operand = fetch_byte(cpu_register);

            cpu_register->status_register.carry = (cpu_register->x_index >= operand)
            ? 1 : 0;

            uint8_t temp_res = cpu_register->x_index - operand;

            // update flags
            update_nz_flags(cpu_register, temp_res);
            break;
        }
    case 0xC0: // CPY Immediate
        {
            uint8_t operand = fetch_byte(cpu_register);

            cpu_register->status_register.carry = (cpu_register->y_index >= operand)
            ? 1 : 0;

            uint8_t temp_res = cpu_register->y_index - operand;

            // update flags
            update_nz_flags(cpu_register, temp_res);
            break;
        }
    case 0x38: // SEC Implied
        {
            cpu_register->status_register.carry = 1;
            tick(); // internal register mod
            break;
        }
    case 0x58: // CLI Implied
        {
            cpu_register->status_register.id = 0;
            tick(); // internal register mod
            break;
        }
    case 0x78: // SEI Implied
        {
            cpu_register->status_register.id = 1;
            tick(); // internal register mod
            break;
        }
    case 0xD8: // CLD Implied
        {
            cpu_register->status_register.dm = 0;
            tick(); // internal register mod
            break;
        }
    case 0xF8: // SED Implied
        {
            cpu_register->status_register.dm = 1;
            tick(); // internal register mod
            break;
        }
    case 0xB8: // CLV Implied
        {
            cpu_register->status_register.overflow = 0;
            tick(); // internal register mod
            break;
        }
    case 0x00: // BRK Implied
        {
            // advance pc past the padding byte required by the specs
            cpu_register->program_counter++;
            tick(); // internal hardware prep cycle

            // push pc high byte into pg 1 stack
            uint16_t pc_push = cpu_register->program_counter;
            write_bytes((uint16_t)(0x0100 + cpu_register->stack_pointer),
                (uint8_t)((pc_push >> 8) & 0xFF));
            cpu_register->stack_pointer--;
            tick(); // high-byte stack write

            // push pc low byte into pg 1 stack
            // Push PC Low Byte (Missing from your file!)
            write_bytes((uint16_t)(0x0100 + cpu_register->stack_pointer),
                (uint8_t)(pc_push & 0xFF));
            cpu_register->stack_pointer--;
            tick();

            // formulate status byte to write to mem, forcing bit 4 high
            union StatusRegister status_push = cpu_register->status_register;
            status_push.break_cmd = 1;
            status_push.unused = 1; // always set to high on stack pushes

            write_bytes((uint16_t)(0x0100 + cpu_register->stack_pointer), status_push.reg);
            cpu_register->stack_pointer--;
            tick(); // status register stack write

            // hardwire the interrupt disable flag to handlers run interrupted
            cpu_register->status_register.id = 1;

            // fetch handler coords from IRQ/BRK vec
            uint8_t target_low = read_bytes(0xFFFE);
            uint8_t target_high = read_bytes(0xFFFF);

            // two ticks for fetching 16-bit vec from mem
            tick();
            tick();

            cpu_register->program_counter = (uint16_t)(target_high << 8) | target_low;
            break;
        }
    case 0x40: // RTI Implied
        {
            tick(); // internal stack layout prep cycle

            // pull reg byte off stack
            cpu_register->stack_pointer++;
            cpu_register->status_register.reg = read_bytes((uint16_t)(0x0100 + cpu_register->stack_pointer));
            tick(); // pull stat reg byte

            // pull pc low byte off stack
            cpu_register->stack_pointer++;
            uint8_t low_byte = read_bytes((uint16_t)(0x0100 + cpu_register->stack_pointer));
            tick(); // pull pc low byte

            // pull pc high byte off stack
            cpu_register->stack_pointer++;
            uint8_t high_byte = read_bytes((uint16_t)(0x0100 + cpu_register->stack_pointer));
            tick();

            // combine bytes and load into pc
            cpu_register->program_counter = (uint16_t)(high_byte << 8) | low_byte;
            tick(); // internal routing alignment cycle
            break;
        }
    case 0x9A: // TXS Implied
        {
            cpu_register->stack_pointer = cpu_register->x_index;
            tick(); // internal reg transfer cycle
            break; // no flag updates
        }
    case 0xBA: // TSX Implied
        {
            cpu_register->x_index = cpu_register->stack_pointer;
            tick(); // internal reg transfer cycle

            // update flags
            update_nz_flags(cpu_register, cpu_register->x_index);
            break;
        }
    case 0x48: // PHA Implied
        {
            tick(); // internal stack pointer prep delay

            // write accumulator contents to curr stack location on page 1
            write_bytes((uint16_t)(0x0100 + cpu_register->stack_pointer), cpu_register->accumulator);
            cpu_register->stack_pointer--;
            tick(); // write to bus cycle
            break;
        }
    case 0x68: // PLA Implied
        {
            tick(); // internal stack layout prep cycle
            tick(); // internal stack decrement delay cycle

            // pull val from new stack pointer pos
            cpu_register->stack_pointer++;
            cpu_register->accumulator = read_bytes((uint16_t)(0x0100 +
                cpu_register->stack_pointer));
            tick(); // mem read cycle

            // update flags
            update_nz_flags(cpu_register, cpu_register->accumulator);
            break;
        }
    default: ;
    }
}

// fetches the byte(s) from the given cpu register
uint8_t fetch_byte(struct CPURegister* cpu_register)
{
    // read the bytes from the pc register
    uint8_t pc_data = read_bytes(cpu_register->program_counter);

    // update the pc register
    cpu_register->program_counter++;

    // dummy instructions
    tick();

    return pc_data;
}

// gets the absolute address of the given cpu register
static inline uint16_t get_abs_address(struct CPURegister* cpu_register)
{
    uint8_t low_byte = fetch_byte(cpu_register);
    uint8_t high_byte = fetch_byte(cpu_register);
    return (high_byte << 8) | low_byte;
}

// gets the zero-page address of the given cpu register
static inline uint16_t get_zpg_address(struct CPURegister* cpu_register)
{
    return (uint16_t)fetch_byte(cpu_register);
}

// gets the zero-page x address of the given cpu register
static inline uint16_t get_zpg_x_address(struct CPURegister* cpu_register)
{
    // fetch base address offset
    uint8_t base_offset = fetch_byte(cpu_register);

    return (uint16_t)((uint8_t)(base_offset + cpu_register->x_index));
}

// gets the zero-page y address of the given cpu register
static inline uint16_t get_zpg_y_address(struct CPURegister* cpu_register)
{
    // fetch base address
    uint8_t base_offset = fetch_byte(cpu_register);

    return (uint16_t)((uint8_t)(base_offset + cpu_register->y_index));
}

// gets the absolute x address and checks if a page has been crossed
uint16_t get_abs_x_address(struct CPURegister* cpu_register, int* pg_crossed)
{
    uint8_t low_byte = fetch_byte(cpu_register);
    uint8_t high_byte = fetch_byte(cpu_register);
    uint16_t base_address = (uint16_t)(high_byte << 8) | low_byte;

    // calc final target address
    uint16_t target_address = base_address + cpu_register->x_index;

    // check if high byte has changed - if so, that means we crossed the page boundary
    *pg_crossed = ((target_address & 0xFF00) != (base_address & 0xFF00));

    return target_address;
}

// gets the absolute y address and tracks if a page has been crossed
uint16_t get_abs_y_address(struct CPURegister* cpu_register, int* pg_crossed)
{
    uint8_t low_byte = fetch_byte(cpu_register);
    uint8_t high_byte = fetch_byte(cpu_register);
    uint16_t base_address = (uint16_t)(high_byte << 8) | low_byte;

    // calc final target address
    uint16_t target_address = base_address + cpu_register->y_index;

    // check if high byte has changed - if so, that means we crossed the page boundary
    *pg_crossed = ((target_address & 0xFF00) != (base_address & 0xFF00));

    return target_address;
}

// gets the indirect x address from the given cpu register
uint16_t get_indirect_x_address(struct CPURegister* cpu_register)
{
    // fetch the 8-bit base coordinate
    uint8_t base_coord = fetch_byte(cpu_register);

    // add register X using 8-bit math to guarantee wrap around for the low address
    uint8_t low_address_ptr = base_coord + cpu_register->x_index;

    // find high byte address also enforcing the wrap around
    uint8_t high_address_ptr = low_address_ptr + 1;

    // look up target address from page zero mem
    uint8_t low_target = read_bytes((uint16_t)low_address_ptr);
    uint8_t high_target = read_bytes((uint16_t)high_address_ptr);

    // combine into final dest ptr
    return (uint16_t)((high_target << 8) | low_target);
}

// gets indirect y address from the given cpu register and calcs if page boundary was crossed
uint16_t get_indirect_y_address(struct CPURegister* cpu_register, int* pg_crossed)
{
    // fetch 8-bit zero page base coord
    uint8_t base_coord = fetch_byte(cpu_register);

    // wrap around calc for high byte location
    uint8_t base_coord_nxt = base_coord + 1;

    // pull 16-bit base vector address directly from zero page
    uint8_t base_low = read_bytes((uint16_t)base_coord);
    uint8_t base_high = read_bytes((uint16_t)base_coord_nxt);
    uint16_t base_address = (uint16_t)((base_high << 8) | base_low);

    // calc final target address by appending y register offset
    uint16_t target_address = base_address + cpu_register->y_index;

    // calc if a page boundry was hit
    *pg_crossed = ((target_address & 0xFF00) != (base_address & 0xFF00));

    return target_address;
}
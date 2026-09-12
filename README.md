# 🕹️ 6502 CPU Emulator

A true-to-form, cycle-accurate implementation of the classic MOS Technology 6502 8-bit microprocessor written in pure C. 
This project acts as a functional sandbox designed to accurately simulate memory mapping, CPU registers, 
instruction fetching, and bitwise state flag logic.

[![License: MIT](https://shields.io)](https://opensource.org)

---

## 🔎 Technical Architecture

This emulator mirrors the hardware execution layout of the physical 6502 chip, focusing on zero-overhead structures and 
performance:

*   **Virtual Memory Map:** Emulates the full 64KB addressable space via a continuous memory array, tracking special
*   segments including the Zero Page (`$0000-$00FF`), the System Stack (`$0100-$01FF`),
*   and the Interrupt Vectors (`$FFFA-$FFFF`).
*   
*   **Register Simulation:** Tracks internal hardware states using dedicated low-overhead data types for
*   the Accumulator (A), X & Y index registers, Stack Pointer (SP), Program Counter (PC), and status flags.
*   
*   **Instruction Pipeline:** Implements a high-efficiency emulation loop handling instruction decoding,
*   custom addressing modes (Immediate, Zero Page, Absolute, Implied, etc.), and correct cycle-count tracking.
*   
---

## 🛠️ Build and Execution Requirements

### Prerequisites
*   A C compiler supporting standard compilation (e.g., `gcc` or `clang`)
*   CMake (minimum version 3.10 recommended)

### Local Compilation Steps
1. Clone the repository locally:
   ```bash
   git clone https://github.com
   cd 6502-Emulator
   ```
2. Generate build configurations and compile:
   ```bash
   cmake .
   make
   ```

## 📄 License

This repository is distributed under the **MIT License**. See `LICENSE` for complete terms.

# Minima — a 16-bit virtual machine you can understand completely

Minima is a small register machine invented from scratch and brought to life as
a C program. It has its own memory, registers, a stack, and a compact
instruction set. It comes with a two-pass **assembler** (so you can write
programs for it in readable text), a **disassembler**, and a **trace/step** mode
so you can watch every register and memory cell change as each instruction
executes. There is no hidden mechanism here — every part is built in this
repository.

**Authors:** [Dmitriy Kuramshin](https://github.com/Krmsh1n5) · [Kamal Yalchin](https://github.com/Camrado) · [Luis Markus Torres](https://github.com/LuisMarkusTorres)

## Build

```bash
make
```

This produces a single executable, `vm`.

## Run

```bash
./vm examples/fib.asm            # run a program
./vm --dump examples/fib.asm     # show the assembled machine code
./vm --trace examples/fib.asm    # print machine state after each instruction
./vm --step examples/fib.asm     # same, but pause for Enter each step
./vm --watch 15 8 examples/hello.asm   # also show memory cells 15..22 each step
./vm --max 50 program.asm        # stop after 50 instructions (runaway guard)
./vm --help
```

## The machine

Minima is a **von Neumann** machine: code and data share one memory.

| Part      | Size / detail                                                    |
|-----------|------------------------------------------------------------------|
| Memory    | 4096 cells, each a 16-bit word, addressed `0..4095`              |
| Registers | `R0`–`R7`, each 16-bit                                           |
| `PC`      | program counter (word index of the next instruction)            |
| `SP`      | stack pointer; the stack starts at the top of memory and grows down |
| Flags     | `Z` (equal) and `L` (less-than), set only by `CMP` and `CMPI`    |

A program is loaded starting at address `0`, and `PC` begins at `0`. The stack
pointer begins at `4096` (one past the last cell); `PUSH` pre-decrements it and
`POP` post-increments it, so the stack and the program grow toward each other.
Arithmetic is modulo 2^16; `PRINT` and the trace show values as signed 16-bit
integers. `CMP`/`CMPI` compare as signed.

### Instruction encoding

Most instructions are a single 16-bit word:

```
 15        8 7      4 3      0
+-----------+--------+--------+
|  opcode   |   rd   |   rs   |
+-----------+--------+--------+
```

Instructions that need a constant or an address (`LOADI`, `ADDI`, `SUBI`,
`CMPI`, and every jump and `CALL`) are **two words**: the instruction word
above, followed by one 16-bit immediate word.

### The fetch–decode–execute cycle

One step does exactly this: read the word at `PC` and advance `PC`; split it into
`opcode`, `rd`, `rs`; if the opcode takes an immediate, read the next word and
advance `PC` again; then perform the operation, which may write a register,
touch memory, or change `PC`. The loop repeats until a `HALT` or a trap. This is
the whole of `vm_step` in `src/cpu.c`.

### Instruction set

Registers are written `Rd`/`Rs`; `imm` is a constant; `addr` is an address or label.

| Mnemonic        | Effect                                             |
|-----------------|----------------------------------------------------|
| `HALT`          | stop the machine                                   |
| `NOP`           | do nothing                                          |
| `LOADI Rd, imm` | `Rd = imm`                                          |
| `MOV Rd, Rs`    | `Rd = Rs`                                           |
| `LOAD Rd, Rs`   | `Rd = mem[Rs]`                                      |
| `STORE Rd, Rs`  | `mem[Rd] = Rs`                                      |
| `PUSH Rd`       | push `Rd` onto the stack                            |
| `POP Rd`        | pop the top of the stack into `Rd`                 |
| `ADD/SUB/MUL`   | `Rd = Rd op Rs`                                     |
| `DIV/MOD Rd, Rs`| `Rd = Rd op Rs` (traps if `Rs == 0`)               |
| `AND/OR/XOR`    | `Rd = Rd op Rs` (bitwise)                           |
| `SHL/SHR Rd, Rs`| shift `Rd` left/right by `Rs & 15`                 |
| `NOT/NEG Rd`    | bitwise complement / two's-complement negate       |
| `INC/DEC Rd`    | `Rd = Rd ± 1`                                       |
| `ADDI/SUBI Rd, imm` | `Rd = Rd ± imm`                                |
| `CMP Rd, Rs`    | set flags from `Rd − Rs`                            |
| `CMPI Rd, imm`  | set flags from `Rd − imm`                           |
| `JMP addr`      | unconditional jump                                 |
| `JE/JZ addr`    | jump if equal (`Z`)                                |
| `JNE/JNZ addr`  | jump if not equal                                  |
| `JL addr`       | jump if less                                       |
| `JG addr`       | jump if greater                                    |
| `JLE addr`      | jump if less-or-equal                              |
| `JGE addr`      | jump if greater-or-equal                           |
| `CALL addr`     | push return address, jump to `addr`                |
| `RET`           | pop the return address into `PC`                   |
| `PRINT Rd`      | print `Rd` as a signed integer and a newline       |
| `PRINTC Rd`     | print the low byte of `Rd` as an ASCII character   |
| `READ Rd`       | read an integer from standard input into `Rd`      |

### When a program does something that makes no sense

Instead of undefined behaviour, the machine stops with a **trap** that names the
problem and the `PC` where it happened, and exits non-zero. The traps are:
illegal opcode, program counter out of range, division by zero, stack overflow,
stack underflow, invalid register, and memory address out of range.

## Writing programs (assembler syntax)

- **One instruction per line.** Blank lines are ignored.
- **Comments** start with `;` and run to the end of the line.
- **Labels** are a name followed by `:` (e.g. `loop:`). A label may sit on its
  own line or in front of an instruction, and can be used anywhere an address or
  constant is expected.
- **Registers** are `R0`–`R7` (case-insensitive).
- **Numbers** are decimal (`10`, `-3`) or hexadecimal (`0x1F`).
- **Characters** are single-quoted (`'A'`), including `'\n'`, `'\t'`, `'\0'`.
- **Directives:**
  - `.word v1, v2, ...` — place literal words in memory.
  - `.string "text"` — place the characters of `text` followed by a `0`.
  - `.org addr` — continue assembling at address `addr`.

`--dump` disassembles linearly from address 0, so any data placed after your code
(for example a `.string`) is shown decoded as instructions; that is expected.

## Examples

| File                   | Shows                                                      |
|------------------------|-----------------------------------------------------------|
| `examples/fib.asm`     | a loop, `CMP`, and a conditional jump (Fibonacci < 1000)  |
| `examples/factorial.asm` | recursion with `CALL`/`RET` and the stack (computes 5!) |
| `examples/hello.asm`   | memory, indirect `LOAD`, and character output             |
| `examples/add.asm`     | reading input with `READ`                                 |

Try `./vm --trace examples/factorial.asm` and watch `SP` walk down as the
recursion deepens.

## Project structure

```
minima-vm/
├── src/
│   ├── vm.h       machine definitions: opcodes, VM state, traps
│   ├── isa.c      the opcode table, lookups, and disassembler
│   ├── asm.c      the two-pass assembler
│   ├── cpu.c      the fetch-decode-execute core
│   └── main.c     command line: load, assemble, dump, run
├── examples/      sample programs in Minima assembly
├── Makefile
└── README.md
```

## Course

This is a course mini-project for **Data Structures and Algorithms 1** (Computer
Science 1, 11 ECTS) at UFAZ. The course covers fundamental data structures and
their algorithms, implemented in C:

- Arrays
- Queues
- Stacks
- Linked lists
- Trees
- Algorithms used to manage these data structures
- Implementation in the C programming language

This project was built for the Systems / Hardware mini-project option: invent a
simple computer and bring it to life as a program in C, with its own memory,
registers, instruction set, and the tools to program it.

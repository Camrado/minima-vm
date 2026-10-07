#ifndef MINIMA_VM_H
#define MINIMA_VM_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#define MEM_SIZE 4096
#define NUM_REGS 8

enum {
    OP_HALT = 0x00,
    OP_NOP  = 0x01,

    OP_LOADI = 0x10,
    OP_MOV   = 0x11,
    OP_LOAD  = 0x12,
    OP_STORE = 0x13,
    OP_PUSH  = 0x14,
    OP_POP   = 0x15,

    OP_ADD = 0x20,
    OP_SUB = 0x21,
    OP_MUL = 0x22,
    OP_DIV = 0x23,
    OP_MOD = 0x24,
    OP_AND = 0x25,
    OP_OR  = 0x26,
    OP_XOR = 0x27,
    OP_SHL = 0x28,
    OP_SHR = 0x29,
    OP_NOT = 0x2A,
    OP_NEG = 0x2B,
    OP_INC = 0x2C,
    OP_DEC = 0x2D,

    OP_ADDI = 0x30,
    OP_SUBI = 0x31,
    OP_CMPI = 0x32,

    OP_CMP = 0x38,

    OP_JMP = 0x40,
    OP_JE  = 0x41,
    OP_JNE = 0x42,
    OP_JL  = 0x43,
    OP_JG  = 0x44,
    OP_JLE = 0x45,
    OP_JGE = 0x46,

    OP_CALL = 0x50,
    OP_RET  = 0x51,

    OP_PRINT  = 0x60,
    OP_PRINTC = 0x61,
    OP_READ   = 0x62
};

typedef enum {
    K_NONE,
    K_R,
    K_RR,
    K_RIMM,
    K_IMM
} OperandKind;

typedef struct {
    const char *name;
    uint8_t opcode;
    OperandKind kind;
} OpInfo;

enum {
    TRAP_NONE = 0,
    TRAP_BAD_OPCODE,
    TRAP_PC_RANGE,
    TRAP_DIV_ZERO,
    TRAP_STACK_OVERFLOW,
    TRAP_STACK_UNDERFLOW,
    TRAP_BAD_REG,
    TRAP_MEM_RANGE
};

typedef struct {
    uint16_t mem[MEM_SIZE];
    uint16_t reg[NUM_REGS];
    uint16_t pc;
    uint16_t sp;
    int zf;
    int lf;
    int halted;
    int trap;
    long steps;
} VM;

const OpInfo *op_lookup_name(const char *name);
const OpInfo *op_lookup_code(uint8_t opcode);
int disasm_one(const uint16_t *mem, uint16_t addr, char *buf, size_t buflen);

int assemble(const char *src, uint16_t *out_mem, int *out_len,
             char *err, size_t errlen);

void vm_init(VM *vm, const uint16_t *image, int len);
int vm_step(VM *vm);
void vm_print_state(const VM *vm, FILE *out);
const char *trap_name(int trap);

#endif

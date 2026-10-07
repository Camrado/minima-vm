#include "vm.h"
#include <string.h>
#include <strings.h>

static const OpInfo OPS[] = {
    {"HALT",   OP_HALT,   K_NONE},
    {"NOP",    OP_NOP,    K_NONE},
    {"LOADI",  OP_LOADI,  K_RIMM},
    {"MOV",    OP_MOV,    K_RR},
    {"LOAD",   OP_LOAD,   K_RR},
    {"STORE",  OP_STORE,  K_RR},
    {"PUSH",   OP_PUSH,   K_R},
    {"POP",    OP_POP,    K_R},
    {"ADD",    OP_ADD,    K_RR},
    {"SUB",    OP_SUB,    K_RR},
    {"MUL",    OP_MUL,    K_RR},
    {"DIV",    OP_DIV,    K_RR},
    {"MOD",    OP_MOD,    K_RR},
    {"AND",    OP_AND,    K_RR},
    {"OR",     OP_OR,     K_RR},
    {"XOR",    OP_XOR,    K_RR},
    {"SHL",    OP_SHL,    K_RR},
    {"SHR",    OP_SHR,    K_RR},
    {"NOT",    OP_NOT,    K_R},
    {"NEG",    OP_NEG,    K_R},
    {"INC",    OP_INC,    K_R},
    {"DEC",    OP_DEC,    K_R},
    {"ADDI",   OP_ADDI,   K_RIMM},
    {"SUBI",   OP_SUBI,   K_RIMM},
    {"CMPI",   OP_CMPI,   K_RIMM},
    {"CMP",    OP_CMP,    K_RR},
    {"JMP",    OP_JMP,    K_IMM},
    {"JE",     OP_JE,     K_IMM},
    {"JZ",     OP_JE,     K_IMM},
    {"JNE",    OP_JNE,    K_IMM},
    {"JNZ",    OP_JNE,    K_IMM},
    {"JL",     OP_JL,     K_IMM},
    {"JG",     OP_JG,     K_IMM},
    {"JLE",    OP_JLE,    K_IMM},
    {"JGE",    OP_JGE,    K_IMM},
    {"CALL",   OP_CALL,   K_IMM},
    {"RET",    OP_RET,    K_NONE},
    {"PRINT",  OP_PRINT,  K_R},
    {"PRINTC", OP_PRINTC, K_R},
    {"READ",   OP_READ,   K_R}
};

static const size_t OPS_COUNT = sizeof(OPS) / sizeof(OPS[0]);

const OpInfo *op_lookup_name(const char *name) {
    for (size_t i = 0; i < OPS_COUNT; i++)
        if (strcasecmp(name, OPS[i].name) == 0)
            return &OPS[i];
    return NULL;
}

const OpInfo *op_lookup_code(uint8_t opcode) {
    for (size_t i = 0; i < OPS_COUNT; i++)
        if (OPS[i].opcode == opcode)
            return &OPS[i];
    return NULL;
}

int disasm_one(const uint16_t *mem, uint16_t addr, char *buf, size_t buflen) {
    uint16_t word = mem[addr];
    uint8_t opcode = (uint8_t)(word >> 8);
    int rd = (word >> 4) & 0xF;
    int rs = word & 0xF;
    const OpInfo *oi = op_lookup_code(opcode);

    if (!oi) {
        snprintf(buf, buflen, ".word 0x%04X", word);
        return 1;
    }

    switch (oi->kind) {
    case K_NONE:
        snprintf(buf, buflen, "%s", oi->name);
        return 1;
    case K_R:
        snprintf(buf, buflen, "%s R%d", oi->name, rd);
        return 1;
    case K_RR:
        snprintf(buf, buflen, "%s R%d, R%d", oi->name, rd, rs);
        return 1;
    case K_RIMM: {
        uint16_t imm = ((uint16_t)(addr + 1) < MEM_SIZE) ? mem[addr + 1] : 0;
        snprintf(buf, buflen, "%s R%d, %d", oi->name, rd, (int16_t)imm);
        return 2;
    }
    case K_IMM: {
        uint16_t imm = ((uint16_t)(addr + 1) < MEM_SIZE) ? mem[addr + 1] : 0;
        snprintf(buf, buflen, "%s %u", oi->name, imm);
        return 2;
    }
    }
    snprintf(buf, buflen, "?");
    return 1;
}

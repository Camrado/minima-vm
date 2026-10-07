#include "vm.h"
#include <string.h>

void vm_init(VM *vm, const uint16_t *image, int len) {
    memset(vm, 0, sizeof(*vm));
    if (len > MEM_SIZE)
        len = MEM_SIZE;
    for (int i = 0; i < len; i++)
        vm->mem[i] = image[i];
    vm->pc = 0;
    vm->sp = MEM_SIZE;
    vm->halted = 0;
    vm->trap = TRAP_NONE;
    vm->steps = 0;
}

const char *trap_name(int trap) {
    switch (trap) {
    case TRAP_NONE:             return "none";
    case TRAP_BAD_OPCODE:       return "illegal opcode";
    case TRAP_PC_RANGE:         return "program counter out of range";
    case TRAP_DIV_ZERO:         return "division by zero";
    case TRAP_STACK_OVERFLOW:   return "stack overflow";
    case TRAP_STACK_UNDERFLOW:  return "stack underflow";
    case TRAP_BAD_REG:          return "invalid register";
    case TRAP_MEM_RANGE:        return "memory address out of range";
    default:                    return "unknown";
    }
}

static void fault(VM *vm, int trap) {
    vm->trap = trap;
    vm->halted = 1;
}

int vm_step(VM *vm) {
    if (vm->halted)
        return 1;

    if (vm->pc >= MEM_SIZE) {
        fault(vm, TRAP_PC_RANGE);
        return 1;
    }

    uint16_t word = vm->mem[vm->pc++];
    uint8_t opcode = (uint8_t)(word >> 8);
    int rd = (word >> 4) & 0xF;
    int rs = word & 0xF;

    const OpInfo *oi = op_lookup_code(opcode);
    if (!oi) {
        vm->pc--;
        fault(vm, TRAP_BAD_OPCODE);
        return 1;
    }

    if (oi->kind == K_R || oi->kind == K_RR || oi->kind == K_RIMM) {
        if (rd >= NUM_REGS) {
            fault(vm, TRAP_BAD_REG);
            return 1;
        }
    }
    if (oi->kind == K_RR && rs >= NUM_REGS) {
        fault(vm, TRAP_BAD_REG);
        return 1;
    }

    uint16_t imm = 0;
    if (oi->kind == K_RIMM || oi->kind == K_IMM) {
        if (vm->pc >= MEM_SIZE) {
            fault(vm, TRAP_PC_RANGE);
            return 1;
        }
        imm = vm->mem[vm->pc++];
    }

    uint16_t *R = vm->reg;
    uint16_t addr;

    switch (opcode) {
    case OP_HALT:
        vm->halted = 1;
        break;
    case OP_NOP:
        break;

    case OP_LOADI:
        R[rd] = imm;
        break;
    case OP_MOV:
        R[rd] = R[rs];
        break;
    case OP_LOAD:
        addr = R[rs];
        if (addr >= MEM_SIZE) { fault(vm, TRAP_MEM_RANGE); return 1; }
        R[rd] = vm->mem[addr];
        break;
    case OP_STORE:
        addr = R[rd];
        if (addr >= MEM_SIZE) { fault(vm, TRAP_MEM_RANGE); return 1; }
        vm->mem[addr] = R[rs];
        break;
    case OP_PUSH:
        if (vm->sp == 0) { fault(vm, TRAP_STACK_OVERFLOW); return 1; }
        vm->mem[--vm->sp] = R[rd];
        break;
    case OP_POP:
        if (vm->sp >= MEM_SIZE) { fault(vm, TRAP_STACK_UNDERFLOW); return 1; }
        R[rd] = vm->mem[vm->sp++];
        break;

    case OP_ADD: R[rd] = (uint16_t)(R[rd] + R[rs]); break;
    case OP_SUB: R[rd] = (uint16_t)(R[rd] - R[rs]); break;
    case OP_MUL: R[rd] = (uint16_t)(R[rd] * R[rs]); break;
    case OP_DIV:
        if (R[rs] == 0) { fault(vm, TRAP_DIV_ZERO); return 1; }
        R[rd] = (uint16_t)(R[rd] / R[rs]);
        break;
    case OP_MOD:
        if (R[rs] == 0) { fault(vm, TRAP_DIV_ZERO); return 1; }
        R[rd] = (uint16_t)(R[rd] % R[rs]);
        break;
    case OP_AND: R[rd] = (uint16_t)(R[rd] & R[rs]); break;
    case OP_OR:  R[rd] = (uint16_t)(R[rd] | R[rs]); break;
    case OP_XOR: R[rd] = (uint16_t)(R[rd] ^ R[rs]); break;
    case OP_SHL: R[rd] = (uint16_t)(R[rd] << (R[rs] & 15)); break;
    case OP_SHR: R[rd] = (uint16_t)(R[rd] >> (R[rs] & 15)); break;
    case OP_NOT: R[rd] = (uint16_t)(~R[rd]); break;
    case OP_NEG: R[rd] = (uint16_t)(-(int16_t)R[rd]); break;
    case OP_INC: R[rd] = (uint16_t)(R[rd] + 1); break;
    case OP_DEC: R[rd] = (uint16_t)(R[rd] - 1); break;

    case OP_ADDI: R[rd] = (uint16_t)(R[rd] + imm); break;
    case OP_SUBI: R[rd] = (uint16_t)(R[rd] - imm); break;
    case OP_CMPI:
        vm->zf = ((int16_t)R[rd] == (int16_t)imm);
        vm->lf = ((int16_t)R[rd] <  (int16_t)imm);
        break;

    case OP_CMP:
        vm->zf = ((int16_t)R[rd] == (int16_t)R[rs]);
        vm->lf = ((int16_t)R[rd] <  (int16_t)R[rs]);
        break;

    case OP_JMP: vm->pc = imm; break;
    case OP_JE:  if (vm->zf) vm->pc = imm; break;
    case OP_JNE: if (!vm->zf) vm->pc = imm; break;
    case OP_JL:  if (vm->lf) vm->pc = imm; break;
    case OP_JG:  if (!vm->lf && !vm->zf) vm->pc = imm; break;
    case OP_JLE: if (vm->lf || vm->zf) vm->pc = imm; break;
    case OP_JGE: if (!vm->lf) vm->pc = imm; break;

    case OP_CALL:
        if (vm->sp == 0) { fault(vm, TRAP_STACK_OVERFLOW); return 1; }
        vm->mem[--vm->sp] = vm->pc;
        vm->pc = imm;
        break;
    case OP_RET:
        if (vm->sp >= MEM_SIZE) { fault(vm, TRAP_STACK_UNDERFLOW); return 1; }
        vm->pc = vm->mem[vm->sp++];
        break;

    case OP_PRINT:
        printf("%d\n", (int16_t)R[rd]);
        break;
    case OP_PRINTC:
        putchar(R[rd] & 0xFF);
        break;
    case OP_READ: {
        long t;
        if (scanf("%ld", &t) != 1)
            t = 0;
        R[rd] = (uint16_t)t;
        break;
    }
    }

    vm->steps++;
    return vm->halted ? 1 : 0;
}

void vm_print_state(const VM *vm, FILE *out) {
    fprintf(out, "pc=%-4u sp=%-4u  flags[Z=%d L=%d]  ",
            vm->pc, vm->sp, vm->zf, vm->lf);
    for (int i = 0; i < NUM_REGS; i++)
        fprintf(out, "R%d=%-6d", i, (int16_t)vm->reg[i]);
    fprintf(out, "\n");
}

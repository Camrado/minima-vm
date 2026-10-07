#include "vm.h"
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)n, f);
    buf[got] = '\0';
    fclose(f);
    return buf;
}

static void dump_listing(const uint16_t *mem, int len) {
    char buf[64];
    int addr = 0;
    printf("addr  words        instruction\n");
    printf("----  -----------  -----------\n");
    while (addr < len) {
        int words = disasm_one(mem, (uint16_t)addr, buf, sizeof(buf));
        if (words == 2 && addr + 1 < len)
            printf("%04d  %04X %04X    %s\n", addr, mem[addr], mem[addr + 1], buf);
        else
            printf("%04d  %04X         %s\n", addr, mem[addr], buf);
        addr += words;
    }
}

static void usage(const char *prog) {
    printf("Minima - a 16-bit register virtual machine\n\n");
    printf("Usage: %s [options] program.asm\n\n", prog);
    printf("Options:\n");
    printf("  --dump           assemble and print a listing, then exit\n");
    printf("  --trace          print machine state after each instruction\n");
    printf("  --step           like --trace, but pause for Enter each step\n");
    printf("  --watch A N      also show memory cells A..A+N-1 each step\n");
    printf("  --max N          stop after N instructions (default 1000000)\n");
    printf("  --help           show this message\n");
}

int main(int argc, char **argv) {
    const char *path = NULL;
    int do_dump = 0, do_trace = 0, do_step = 0;
    int watch_start = -1, watch_count = 0;
    long max_steps = 1000000;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) { usage(argv[0]); return 0; }
        else if (strcmp(argv[i], "--dump") == 0) do_dump = 1;
        else if (strcmp(argv[i], "--trace") == 0) do_trace = 1;
        else if (strcmp(argv[i], "--step") == 0) { do_step = 1; do_trace = 1; }
        else if (strcmp(argv[i], "--watch") == 0 && i + 2 < argc) {
            watch_start = atoi(argv[++i]);
            watch_count = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--max") == 0 && i + 1 < argc) {
            max_steps = atol(argv[++i]);
        } else if (argv[i][0] != '-') {
            path = argv[i];
        } else {
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            return 2;
        }
    }

    if (!path) {
        usage(argv[0]);
        return 2;
    }

    char *src = read_file(path);
    if (!src) {
        fprintf(stderr, "could not read '%s'\n", path);
        return 2;
    }

    static uint16_t image[MEM_SIZE];
    int len = 0;
    char err[256];
    if (assemble(src, image, &len, err, sizeof(err)) != 0) {
        fprintf(stderr, "assembly error: %s\n", err);
        free(src);
        return 1;
    }
    free(src);

    if (do_dump) {
        dump_listing(image, len);
        return 0;
    }

    VM vm;
    vm_init(&vm, image, len);

    while (!vm.halted && vm.steps < max_steps) {
        char buf[64];
        if (do_trace)
            disasm_one(vm.mem, vm.pc, buf, sizeof(buf));

        vm_step(&vm);

        if (do_trace) {
            fprintf(stderr, "[%04ld] %-20s | ", vm.steps, buf);
            vm_print_state(&vm, stderr);
            if (watch_start >= 0) {
                fprintf(stderr, "        mem[%d..%d] =", watch_start, watch_start + watch_count - 1);
                for (int k = 0; k < watch_count && watch_start + k < MEM_SIZE; k++)
                    fprintf(stderr, " %d", (int16_t)vm.mem[watch_start + k]);
                fprintf(stderr, "\n");
            }
            if (do_step) {
                int c;
                while ((c = getchar()) != '\n' && c != EOF)
                    ;
            }
        }
    }

    if (vm.trap != TRAP_NONE) {
        fprintf(stderr, "\n*** trap: %s (at pc=%u) ***\n", trap_name(vm.trap), vm.pc);
        return 1;
    }
    if (vm.steps >= max_steps) {
        fprintf(stderr, "\n*** stopped: instruction limit (%ld) reached ***\n", max_steps);
        return 1;
    }
    return 0;
}

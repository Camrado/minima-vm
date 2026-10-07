#include "vm.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>

#define MAX_LABELS 512
#define MAX_TOKENS 8

typedef struct {
    char name[64];
    int addr;
} Label;

typedef struct {
    Label items[MAX_LABELS];
    int count;
} LabelTable;

static int label_add(LabelTable *t, const char *name, int addr) {
    if (t->count >= MAX_LABELS)
        return -1;
    for (int i = 0; i < t->count; i++)
        if (strcasecmp(t->items[i].name, name) == 0)
            return -1;
    strncpy(t->items[t->count].name, name, sizeof(t->items[0].name) - 1);
    t->items[t->count].name[sizeof(t->items[0].name) - 1] = '\0';
    t->items[t->count].addr = addr;
    t->count++;
    return 0;
}

static int label_find(const LabelTable *t, const char *name, int *out) {
    for (int i = 0; i < t->count; i++)
        if (strcasecmp(t->items[i].name, name) == 0) {
            *out = t->items[i].addr;
            return 0;
        }
    return -1;
}

static int parse_reg(const char *tok) {
    if ((tok[0] == 'R' || tok[0] == 'r') && isdigit((unsigned char)tok[1]) && tok[2] == '\0')
        return tok[1] - '0';
    return -1;
}

static int parse_escape(const char *s, int *val) {
    switch (s[0]) {
    case 'n': *val = '\n'; return 1;
    case 't': *val = '\t'; return 1;
    case 'r': *val = '\r'; return 1;
    case '0': *val = '\0'; return 1;
    case '\\': *val = '\\'; return 1;
    case '\'': *val = '\''; return 1;
    case '"': *val = '"'; return 1;
    default: return 0;
    }
}

static int parse_value(const char *tok, const LabelTable *labels,
                       int pass, int *out) {
    if (tok[0] == '\'') {
        if (tok[1] == '\\') {
            int v;
            if (parse_escape(tok + 2, &v) && tok[3] == '\'') { *out = v; return 0; }
            return -1;
        }
        if (tok[1] != '\0' && tok[2] == '\'') { *out = (unsigned char)tok[1]; return 0; }
        return -1;
    }
    if (isalpha((unsigned char)tok[0]) || tok[0] == '_' || tok[0] == '.') {
        int addr;
        if (label_find(labels, tok, &addr) == 0) { *out = addr; return 0; }
        if (pass == 1) { *out = 0; return 0; }
        return -1;
    }
    {
        char *end;
        long v = strtol(tok, &end, 0);
        if (*end != '\0')
            return -1;
        *out = (int)v;
        return 0;
    }
}

static int tokenize(char *line, char *toks[], int max) {
    int n = 0;
    char *p = line;
    while (*p && n < max) {
        while (*p == ' ' || *p == '\t' || *p == ',')
            p++;
        if (*p == '\0')
            break;
        toks[n++] = p;
        while (*p && *p != ' ' && *p != '\t' && *p != ',')
            p++;
        if (*p)
            *p++ = '\0';
    }
    return n;
}

static void strip_comment(char *line) {
    for (char *p = line; *p; p++)
        if (*p == ';') { *p = '\0'; return; }
}

static int emit_string(const char *rest, uint16_t *out_mem, int *lc,
                       int pass, char *err, size_t errlen) {
    const char *p = strchr(rest, '"');
    if (!p) { snprintf(err, errlen, "expected \" in .string"); return -1; }
    p++;
    while (*p && *p != '"') {
        int ch;
        if (*p == '\\') {
            p++;
            if (!parse_escape(p, &ch)) { snprintf(err, errlen, "bad escape in .string"); return -1; }
            p++;
        } else {
            ch = (unsigned char)*p++;
        }
        if (pass == 2)
            out_mem[*lc] = (uint16_t)ch;
        (*lc)++;
    }
    if (*p != '"') { snprintf(err, errlen, "unterminated .string"); return -1; }
    if (pass == 2)
        out_mem[*lc] = 0;
    (*lc)++;
    return 0;
}

static int assemble_pass(const char *src, uint16_t *out_mem, int *out_len,
                         LabelTable *labels, int pass,
                         char *err, size_t errlen) {
    char line[512];
    int lc = 0;
    int maxlc = 0;
    int lineno = 0;
    const char *s = src;

    while (*s) {
        const char *nl = strchr(s, '\n');
        size_t len = nl ? (size_t)(nl - s) : strlen(s);
        if (len >= sizeof(line))
            len = sizeof(line) - 1;
        memcpy(line, s, len);
        line[len] = '\0';
        s = nl ? nl + 1 : s + strlen(s);
        lineno++;

        strip_comment(line);

        char raw[512];
        strncpy(raw, line, sizeof(raw) - 1);
        raw[sizeof(raw) - 1] = '\0';

        char *toks[MAX_TOKENS];
        int nt = tokenize(line, toks, MAX_TOKENS);
        int ti = 0;

        while (ti < nt) {
            size_t tl = strlen(toks[ti]);
            if (tl > 0 && toks[ti][tl - 1] == ':') {
                toks[ti][tl - 1] = '\0';
                if (pass == 1 && label_add(labels, toks[ti], lc) != 0) {
                    snprintf(err, errlen, "line %d: duplicate label '%s'", lineno, toks[ti]);
                    return -1;
                }
                ti++;
            } else {
                break;
            }
        }
        if (ti >= nt)
            continue;

        char *mnem = toks[ti++];

        if (strcasecmp(mnem, ".org") == 0) {
            int v;
            if (ti >= nt || parse_value(toks[ti], labels, pass, &v) != 0) {
                snprintf(err, errlen, "line %d: .org needs an address", lineno);
                return -1;
            }
            lc = v;
            if (lc > maxlc) maxlc = lc;
            continue;
        }
        if (strcasecmp(mnem, ".word") == 0) {
            if (ti >= nt) { snprintf(err, errlen, "line %d: .word needs values", lineno); return -1; }
            for (; ti < nt; ti++) {
                int v;
                if (parse_value(toks[ti], labels, pass, &v) != 0) {
                    snprintf(err, errlen, "line %d: bad value '%s'", lineno, toks[ti]);
                    return -1;
                }
                if (pass == 2) out_mem[lc] = (uint16_t)v;
                lc++;
            }
            if (lc > maxlc) maxlc = lc;
            continue;
        }
        if (strcasecmp(mnem, ".string") == 0) {
            const char *rest = strchr(raw, '"');
            if (emit_string(rest ? rest : "", out_mem, &lc, pass, err, errlen) != 0) {
                char tmp[128];
                snprintf(tmp, sizeof(tmp), "line %d: %s", lineno, err);
                strncpy(err, tmp, errlen - 1);
                err[errlen - 1] = '\0';
                return -1;
            }
            if (lc > maxlc) maxlc = lc;
            continue;
        }

        const OpInfo *oi = op_lookup_name(mnem);
        if (!oi) {
            snprintf(err, errlen, "line %d: unknown instruction '%s'", lineno, mnem);
            return -1;
        }

        int rd = 0, rs = 0, imm = 0;
        int need = nt - ti;

        switch (oi->kind) {
        case K_NONE:
            if (need != 0) { snprintf(err, errlen, "line %d: %s takes no operands", lineno, mnem); return -1; }
            break;
        case K_R:
            if (need != 1 || (rd = parse_reg(toks[ti])) < 0) {
                snprintf(err, errlen, "line %d: %s needs a register", lineno, mnem); return -1;
            }
            break;
        case K_RR:
            if (need != 2 || (rd = parse_reg(toks[ti])) < 0 || (rs = parse_reg(toks[ti + 1])) < 0) {
                snprintf(err, errlen, "line %d: %s needs two registers", lineno, mnem); return -1;
            }
            break;
        case K_RIMM:
            if (need != 2 || (rd = parse_reg(toks[ti])) < 0 ||
                parse_value(toks[ti + 1], labels, pass, &imm) != 0) {
                snprintf(err, errlen, "line %d: %s needs a register and a value", lineno, mnem); return -1;
            }
            break;
        case K_IMM:
            if (need != 1 || parse_value(toks[ti], labels, pass, &imm) != 0) {
                snprintf(err, errlen, "line %d: %s needs an address", lineno, mnem); return -1;
            }
            break;
        }

        if (pass == 2)
            out_mem[lc] = (uint16_t)((oi->opcode << 8) | (rd << 4) | rs);
        lc++;
        if (oi->kind == K_RIMM || oi->kind == K_IMM) {
            if (pass == 2)
                out_mem[lc] = (uint16_t)imm;
            lc++;
        }
        if (lc > maxlc) maxlc = lc;
    }

    *out_len = maxlc;
    return 0;
}

int assemble(const char *src, uint16_t *out_mem, int *out_len,
             char *err, size_t errlen) {
    LabelTable labels;
    labels.count = 0;
    int len1 = 0;

    for (int i = 0; i < MEM_SIZE; i++)
        out_mem[i] = 0;

    if (assemble_pass(src, out_mem, &len1, &labels, 1, err, errlen) != 0)
        return -1;
    if (assemble_pass(src, out_mem, out_len, &labels, 2, err, errlen) != 0)
        return -1;
    return 0;
}

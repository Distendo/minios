#include "tinycc.h"
#include "asm.h"
#include "vga.h"
#include "fs.h"
#include <stdint.h>

#define MAX_ASM 4096
static char asm_buf[MAX_ASM];
static int asm_pos;

static void asm_emit(const char *s) {
    while (*s && asm_pos < MAX_ASM - 1) asm_buf[asm_pos++] = *s++;
}

static void asm_emit_n(int v) {
    if (v < 0) { asm_buf[asm_pos++] = '-'; v = -v; }
    int d = 1, t = v;
    while (t >= 10) { d *= 10; t /= 10; }
    do { asm_buf[asm_pos++] = '0' + v / d; v %= d; d /= 10; } while (d);
}

// C Lexer
enum { C_EOF, C_ID, C_NUM, C_STR, C_SEMI, C_LBRACE, C_RBRACE, C_LPAREN, C_RPAREN,
       C_EQ, C_PLUS, C_MINUS, C_STAR, C_SLASH, C_PERCENT,
       C_EQEQ, C_NE, C_LT, C_GT, C_LE, C_GE, C_EXCLAM,
       C_COMMA, C_AMPER, C_PIPE };

typedef struct { int type; int ival; char text[64]; } ctoken_t;

static const char *c_src;
static ctoken_t c_tok;
static int label_count;
static int str_count;

static void c_next(void) {
    while (*c_src == ' ' || *c_src == '\t') c_src++;
    if (*c_src == 0) { c_tok.type = C_EOF; return; }
    if (*c_src == '/' && *(c_src + 1) == '/') {
        while (*c_src && *c_src != '\n') c_src++;
        if (*c_src == '\n') { c_src++; c_next(); return; }
        c_tok.type = C_EOF; return;
    }
    if (*c_src == '/' && *(c_src + 1) == '*') {
        c_src += 2;
        while (*c_src && !(*c_src == '*' && *(c_src + 1) == '/')) c_src++;
        if (*c_src == '*') c_src += 2;
        c_next(); return;
    }
    if (*c_src == '\n') { c_src++; c_next(); return; }
    if (*c_src == ';') { c_src++; c_tok.type = C_SEMI; return; }
    if (*c_src == '{') { c_src++; c_tok.type = C_LBRACE; return; }
    if (*c_src == '}') { c_src++; c_tok.type = C_RBRACE; return; }
    if (*c_src == '(') { c_src++; c_tok.type = C_LPAREN; return; }
    if (*c_src == ')') { c_src++; c_tok.type = C_RPAREN; return; }
    if (*c_src == ',') { c_src++; c_tok.type = C_COMMA; return; }
    if (*c_src == '=') {
        c_src++;
        if (*c_src == '=') { c_src++; c_tok.type = C_EQEQ; return; }
        c_tok.type = C_EQ; return;
    }
    if (*c_src == '!') {
        if (*(c_src + 1) == '=') { c_src += 2; c_tok.type = C_NE; return; }
        c_src++; c_tok.type = C_EXCLAM; return;
    }
    if (*c_src == '<') {
        c_src++;
        if (*c_src == '=') { c_src++; c_tok.type = C_LE; return; }
        c_tok.type = C_LT; return;
    }
    if (*c_src == '>') {
        c_src++;
        if (*c_src == '=') { c_src++; c_tok.type = C_GE; return; }
        c_tok.type = C_GT; return;
    }
    if (*c_src == '+' && *(c_src + 1) != '+') { c_src++; c_tok.type = C_PLUS; return; }
    if (*c_src == '-') { c_src++; c_tok.type = C_MINUS; return; }
    if (*c_src == '*') { c_src++; c_tok.type = C_STAR; return; }
    if (*c_src == '/') { c_src++; c_tok.type = C_SLASH; return; }
    if (*c_src == '%') { c_src++; c_tok.type = C_PERCENT; return; }
    if (*c_src == '"') {
        c_src++; int i = 0; c_tok.type = C_STR;
        while (*c_src && *c_src != '"' && i < 63) c_tok.text[i++] = *c_src++;
        c_tok.text[i] = 0;
        if (*c_src == '"') c_src++;
        return;
    }
    if (*c_src >= '0' && *c_src <= '9') {
        c_tok.type = C_NUM; c_tok.ival = 0;
        while (*c_src >= '0' && *c_src <= '9') {
            c_tok.ival = c_tok.ival * 10 + (*c_src - '0');
            c_src++;
        }
        return;
    }
    if ((*c_src >= 'a' && *c_src <= 'z') || (*c_src >= 'A' && *c_src <= 'Z') || *c_src == '_') {
        int i = 0; c_tok.type = C_ID;
        while ((*c_src >= 'a' && *c_src <= 'z') || (*c_src >= 'A' && *c_src <= 'Z') ||
               (*c_src >= '0' && *c_src <= '9') || *c_src == '_') {
            if (i < 63) c_tok.text[i++] = *c_src;
            c_src++;
        }
        c_tok.text[i] = 0;
        return;
    }
    c_tok.type = C_EOF;
}

static int c_kw(const char *kw) {
    const char *t = c_tok.text;
    while (*kw && *t && *kw == *t) { kw++; t++; }
    return *kw == 0 && *t == 0;
}

static void parse_statement(void);
static void parse_primary(void);
static void parse_mul(void);
static void parse_expr(void);

// Variable tracking
#define MAX_VARS 64
static char var_names[MAX_VARS][32];
static int var_count;

static int find_cvar(const char *name) {
    for (int i = 0; i < var_count; i++) {
        int j = 0;
        while (var_names[i][j] && name[j] && var_names[i][j] == name[j]) j++;
        if (var_names[i][j] == 0 && name[j] == 0) return i;
    }
    return -1;
}

static int add_var(const char *name) {
    if (find_cvar(name) >= 0) return 0;
    if (var_count >= MAX_VARS) return 0;
    int j = 0;
    while (name[j] && j < 31) { var_names[var_count][j] = name[j]; j++; }
    var_names[var_count][j] = 0;
    var_count++;
    return 1;
}

static int new_label(void) {
    return label_count++;
}

static void emit_label(int n) {
    asm_emit("_L"); asm_emit_n(n); asm_emit(":\n");
}

static void parse_primary(void) {
    if (c_tok.type == C_NUM) {
        asm_emit("    mov eax, "); asm_emit_n(c_tok.ival); asm_emit("\n");
        c_next();
    } else if (c_tok.type == C_LPAREN) {
        c_next(); parse_expr();
        if (c_tok.type == C_RPAREN) c_next();
    } else if (c_tok.type == C_MINUS) {
        c_next(); parse_primary();
        asm_emit("    neg eax\n");
    } else if (c_tok.type == C_EXCLAM) {
        c_next(); parse_primary();
        asm_emit("    cmp eax, 0\n    sete al\n    and eax, 1\n");
    } else if (c_tok.type == C_ID) {
        char name[64]; int ni = 0;
        while (c_tok.text[ni] && ni < 63) { name[ni] = c_tok.text[ni]; ni++; }
        name[ni] = 0;
        c_next();
        if (c_tok.type == C_LPAREN) {
            c_next();
            if (name[0]=='p' && name[1]=='r' && name[2]=='i' && name[3]=='n' && name[4]=='t' && name[5]=='_') {
                if (name[6]=='s' && name[7]=='t' && name[8]=='r' && name[9]==0) {
                    if (c_tok.type == C_STR) {
                        int si = str_count++;
                        asm_emit("_S"); asm_emit_n(si);
                        asm_emit(": db \""); asm_emit(c_tok.text); asm_emit("\", 0\n");
                        c_next();
                    }
                } else if (name[6]=='i' && name[7]=='n' && name[8]=='t' && name[9]==0) {
                    parse_expr();
                }
                if (c_tok.type == C_RPAREN) c_next();
                if (name[6]=='s' && name[7]=='t' && name[8]=='r' && name[9]==0) {
                    asm_emit("    mov [__io_ptr], eax\n");
                    asm_emit("    mov eax, [__io_table + 0]\n    call eax\n");
                } else if (name[6]=='i' && name[7]=='n' && name[8]=='t' && name[9]==0) {
                    asm_emit("    mov [__io_ptr], eax\n");
                    asm_emit("    mov eax, [__io_table + 8]\n    call eax\n");
                } else if (name[6]=='n' && name[7]=='l' && name[8]==0) {
                    asm_emit("    mov eax, [__io_table + 12]\n    call eax\n");
                }
                return;
            }
            while (c_tok.type != C_RPAREN && c_tok.type != C_EOF) c_next();
            if (c_tok.type == C_RPAREN) c_next();
            return;
        }
        int vi = find_cvar(name);
        if (vi >= 0) {
            asm_emit("    mov eax, [_var_"); asm_emit(name); asm_emit("]\n");
        } else {
            asm_emit("    xor eax, eax\n");
        }
    } else {
        asm_emit("    xor eax, eax\n");
        if (c_tok.type != C_EOF) c_next();
    }
}

static void parse_mul(void) {
    parse_primary();
    while (c_tok.type == C_STAR || c_tok.type == C_SLASH || c_tok.type == C_PERCENT) {
        int op = c_tok.type; c_next();
        asm_emit("    push eax\n");
        parse_primary();
        asm_emit("    mov ebx, eax\n    pop eax\n");
        if (op == C_STAR) asm_emit("    imul eax, ebx\n");
        else if (op == C_SLASH) asm_emit("    xor edx, edx\n    idiv ebx\n");
        else asm_emit("    xor edx, edx\n    idiv ebx\n    mov eax, edx\n");
    }
}

static void parse_expr(void) {
    parse_mul();
    while (c_tok.type == C_PLUS || c_tok.type == C_MINUS) {
        int op = c_tok.type; c_next();
        asm_emit("    push eax\n");
        parse_mul();
        asm_emit("    mov ebx, eax\n    pop eax\n");
        if (op == C_PLUS) asm_emit("    add eax, ebx\n");
        else asm_emit("    sub eax, ebx\n");
    }
    // Handle comparisons
    if (c_tok.type == C_EQEQ || c_tok.type == C_NE || c_tok.type == C_LT ||
        c_tok.type == C_GT || c_tok.type == C_LE || c_tok.type == C_GE) {
        int op = c_tok.type; c_next();
        asm_emit("    push eax\n");
        parse_expr();
        asm_emit("    mov ebx, eax\n    pop eax\n");
        asm_emit("    cmp eax, ebx\n");
        int lc = new_label();
        if (op == C_EQEQ) asm_emit("    je _T"); else if (op == C_NE) asm_emit("    jne _T");
        else if (op == C_LT) asm_emit("    jl _T");
        else if (op == C_GT) asm_emit("    jg _T");
        else if (op == C_LE) asm_emit("    jle _T");
        else asm_emit("    jge _T");
        asm_emit_n(lc); asm_emit("\n");
        asm_emit("    mov eax, 0\n    jmp _F");
        asm_emit_n(lc); asm_emit("\n");
        asm_emit("_T"); asm_emit_n(lc); asm_emit(":\n");
        asm_emit("    mov eax, 1\n");
        asm_emit("_F"); asm_emit_n(lc); asm_emit(":\n");
    }
}

static void parse_block(void);

static void parse_statement(void) {
    if (c_tok.type == C_SEMI) { c_next(); return; }
    if (c_tok.type == C_LBRACE) { c_next(); parse_block(); if (c_tok.type == C_RBRACE) c_next(); return; }

    if (c_tok.type == C_ID && c_kw("if")) {
        c_next();
        if (c_tok.type == C_LPAREN) c_next();
        int lc = new_label();
        parse_expr();
        asm_emit("    cmp eax, 0\n    je _F"); asm_emit_n(lc); asm_emit("\n");
        if (c_tok.type == C_RPAREN) c_next();
        parse_statement();
        int has_else = 0;
        if (c_tok.type == C_ID && c_kw("else")) { has_else = 1; c_next(); }
        if (has_else) asm_emit("    jmp _E"); asm_emit_n(lc); asm_emit("\n");
        asm_emit("_F"); asm_emit_n(lc); asm_emit(":\n");
        if (has_else) { parse_statement(); asm_emit("_E"); asm_emit_n(lc); asm_emit(":\n"); }
        return;
    }

    if (c_tok.type == C_ID && c_kw("while")) {
        c_next();
        if (c_tok.type == C_LPAREN) c_next();
        int lc = new_label();
        asm_emit("_W"); asm_emit_n(lc); asm_emit(":\n");
        parse_expr();
        if (c_tok.type == C_RPAREN) c_next();
        asm_emit("    cmp eax, 0\n    je _E"); asm_emit_n(lc); asm_emit("\n");
        parse_statement();
        asm_emit("    jmp _W"); asm_emit_n(lc); asm_emit("\n");
        asm_emit("_E"); asm_emit_n(lc); asm_emit(":\n");
        return;
    }

    if (c_tok.type == C_ID && c_kw("return")) {
        c_next(); parse_expr();
        asm_emit("    pop ebx\n    ret\n");
        if (c_tok.type == C_SEMI) c_next();
        return;
    }

    if (c_tok.type == C_ID && c_kw("int")) {
        c_next();
        while (c_tok.type == C_ID || c_tok.type == C_COMMA) {
            if (c_tok.type == C_COMMA) { c_next(); continue; }
            char name[64]; int ni = 0;
            while (c_tok.text[ni] && ni < 63) { name[ni] = c_tok.text[ni]; ni++; }
            name[ni] = 0; c_next();
            add_var(name);
            if (c_tok.type == C_EQ) { c_next(); parse_expr(); }
            else { asm_emit("    xor eax, eax\n"); }
            asm_emit("    mov [_var_"); asm_emit(name); asm_emit("], eax\n");
        }
        if (c_tok.type == C_SEMI) c_next();
        return;
    }

    if (c_tok.type == C_ID) {
        char name[64]; int ni = 0;
        while (c_tok.text[ni] && ni < 63) { name[ni] = c_tok.text[ni]; ni++; }
        name[ni] = 0; c_next();

        if (c_tok.type == C_EQ) {
            int vi = find_cvar(name);
            if (vi < 0) add_var(name);
            c_next(); parse_expr();
            asm_emit("    mov [_var_"); asm_emit(name); asm_emit("], eax\n");
            if (c_tok.type == C_SEMI) c_next();
            return;
        }

        if (c_tok.type == C_LPAREN) {
            c_next();
            if (name[0]=='p' && name[1]=='r' && name[2]=='i' && name[3]=='n' && name[4]=='t' && name[5]=='_') {
                if (name[6]=='s' && name[7]=='t' && name[8]=='r' && name[9]==0 && c_tok.type == C_STR) {
                    int si = str_count++;
                    asm_emit("_S"); asm_emit_n(si);
                    asm_emit(": db \""); asm_emit(c_tok.text); asm_emit("\", 0\n");
                    c_next();
                    asm_emit("    mov eax, _S"); asm_emit_n(si); asm_emit("\n");
                    asm_emit("    mov [__io_ptr], eax\n");
                    asm_emit("    mov eax, [__io_table + 0]\n    call eax\n");
                } else if (name[6]=='i' && name[7]=='n' && name[8]=='t' && name[9]==0) {
                    parse_expr();
                    asm_emit("    mov [__io_ptr], eax\n");
                    asm_emit("    mov eax, [__io_table + 8]\n    call eax\n");
                } else if (name[6]=='n' && name[7]=='l' && name[8]==0) {
                    asm_emit("    mov eax, [__io_table + 12]\n    call eax\n");
                }
            }
            while (c_tok.type != C_RPAREN && c_tok.type != C_EOF) c_next();
            if (c_tok.type == C_RPAREN) c_next();
            if (c_tok.type == C_SEMI) c_next();
            return;
        }
        while (c_tok.type != C_SEMI && c_tok.type != C_EOF) c_next();
        if (c_tok.type == C_SEMI) c_next();
        return;
    }

    while (c_tok.type != C_SEMI && c_tok.type != C_EOF) c_next();
    if (c_tok.type == C_SEMI) c_next();
}

static void parse_block(void) {
    while (c_tok.type != C_RBRACE && c_tok.type != C_EOF) parse_statement();
}

static void parse_program(void) {
    while (c_tok.type != C_EOF) {
        if (c_tok.type == C_ID && c_kw("int")) {
            c_next();
            if (c_tok.type == C_ID) {
                char name[64]; int ni = 0;
                while (c_tok.text[ni] && ni < 63) { name[ni] = c_tok.text[ni]; ni++; }
                name[ni] = 0; c_next();
                if (c_tok.type == C_LPAREN) {
                    c_next();
                    if (c_tok.type == C_ID) c_next();
                    if (c_tok.type == C_RPAREN) c_next();
                    if (c_tok.type == C_LBRACE) {
                        c_next();
                        asm_emit("_start:\n");
                        asm_emit("    push ebx\n");
                        parse_block();
                        if (c_tok.type == C_RBRACE) c_next();
                        asm_emit("    pop ebx\n    ret\n");
                    }
                } else {
                    add_var(name);
                    if (c_tok.type == C_EQ) { c_next(); parse_expr(); }
                    else { asm_emit("    xor eax, eax\n"); }
                    asm_emit("    mov [_var_"); asm_emit(name); asm_emit("], eax\n");
                    if (c_tok.type == C_COMMA) { continue; }
                    if (c_tok.type == C_SEMI) c_next();
                }
            }
        } else {
            c_next();
        }
    }
}

int tcc_compile(const char *source, char *asm_out, int max_out) {
    c_src = source;
    label_count = 0;
    str_count = 0;
    var_count = 0;
    asm_pos = 0;
    asm_buf[0] = 0;

    asm_emit("__io_table:\n");
    asm_emit("dd 0\n"); asm_emit("dd 0\n"); asm_emit("dd 0\n"); asm_emit("dd 0\n");
    asm_emit("__io_ptr:\ndd 0\n");
    asm_emit("jmp _start\n");

    c_next();
    parse_program();

    asm_emit("; Variables:\n");
    for (int i = 0; i < var_count; i++) {
        asm_emit("_var_"); asm_emit(var_names[i]); asm_emit(": resb 4\n");
    }
    asm_buf[asm_pos] = 0;

    if (asm_out && max_out > 0) {
        int i;
        for (i = 0; i < asm_pos && i < max_out - 1; i++)
            asm_out[i] = asm_buf[i];
        asm_out[i] = 0;
    }
    return asm_pos;
}

int tcc_build(const char *source, const char *outname) {
    int len = tcc_compile(source, 0, 0);
    if (len <= 0) return 0;
    uint8_t bin[4096];
    int bin_size = asm_assemble(asm_buf, bin, 4096, outname);
    if (bin_size <= 0 || (bin[0] == 0xFF && bin[1] == 0xFF)) return 0;
    if (!fs_write(outname, bin, bin_size)) return 0;
    return bin_size;
}

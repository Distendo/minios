#include "pylang.h"
#include "vga.h"
#include <stdint.h>

enum {
    T_EOF, T_NUM, T_ID, T_STR,
    T_PLUS, T_MINUS, T_STAR, T_SLASH, T_PERCENT,
    T_EQ, T_EQEQ, T_NE, T_LT, T_GT, T_LE, T_GE,
    T_LPAREN, T_RPAREN, T_COMMA, T_COLON
};

typedef struct { int type; int ival; char text[64]; } token_t;

#define MAX_VARS 64
typedef struct { char name[32]; int val; } var_t;

static var_t vars[MAX_VARS];
static int var_n;
static token_t tok;
static const char *line_p;

static int find_var(const char *name) {
    for (int i = 0; i < var_n; i++) {
        int j = 0;
        while (vars[i].name[j] && name[j] && vars[i].name[j] == name[j]) j++;
        if (vars[i].name[j] == 0 && name[j] == 0) return i;
    }
    return -1;
}

static void set_var(const char *name, int val) {
    int i = find_var(name);
    if (i >= 0) { vars[i].val = val; return; }
    if (var_n >= MAX_VARS) return;
    int j = 0;
    while (name[j] && j < 31) { vars[var_n].name[j] = name[j]; j++; }
    vars[var_n].name[j] = 0;
    vars[var_n].val = val;
    var_n++;
}

static int get_var(const char *name) {
    int i = find_var(name);
    return (i >= 0) ? vars[i].val : 0;
}

static int parse_primary(void);
static int parse_mul(void);
static int parse_expr(void);
static int parse_cmp(void);

static void lex_next(void) {
    while (*line_p == ' ' || *line_p == '\t') line_p++;
    if (*line_p == 0) { tok.type = T_EOF; return; }
    if (*line_p == '#') { tok.type = T_EOF; return; }
    if (*line_p == '+') { line_p++; tok.type = T_PLUS; return; }
    if (*line_p == '-') { line_p++; tok.type = T_MINUS; return; }
    if (*line_p == '*') { line_p++; tok.type = T_STAR; return; }
    if (*line_p == '/') { line_p++; tok.type = T_SLASH; return; }
    if (*line_p == '%') { line_p++; tok.type = T_PERCENT; return; }
    if (*line_p == '(') { line_p++; tok.type = T_LPAREN; return; }
    if (*line_p == ')') { line_p++; tok.type = T_RPAREN; return; }
    if (*line_p == ',') { line_p++; tok.type = T_COMMA; return; }
    if (*line_p == ':') { line_p++; tok.type = T_COLON; return; }
    if (*line_p == '=') {
        line_p++;
        if (*line_p == '=') { line_p++; tok.type = T_EQEQ; return; }
        tok.type = T_EQ; return;
    }
    if (*line_p == '!') {
        if (*(line_p + 1) == '=') { line_p += 2; tok.type = T_NE; return; }
        line_p++; tok.type = T_EOF; return;
    }
    if (*line_p == '<') {
        line_p++;
        if (*line_p == '=') { line_p++; tok.type = T_LE; return; }
        tok.type = T_LT; return;
    }
    if (*line_p == '>') {
        line_p++;
        if (*line_p == '=') { line_p++; tok.type = T_GE; return; }
        tok.type = T_GT; return;
    }
    if (*line_p == '"' || *line_p == '\'') {
        char q = *line_p; line_p++;
        int i = 0; tok.type = T_STR;
        while (*line_p && *line_p != q && i < 63) tok.text[i++] = *line_p++;
        tok.text[i] = 0;
        if (*line_p == q) line_p++;
        return;
    }
    if (*line_p >= '0' && *line_p <= '9') {
        tok.type = T_NUM; tok.ival = 0;
        while (*line_p >= '0' && *line_p <= '9') {
            tok.ival = tok.ival * 10 + (*line_p - '0');
            line_p++;
        }
        return;
    }
    if ((*line_p >= 'a' && *line_p <= 'z') || (*line_p >= 'A' && *line_p <= 'Z') || *line_p == '_') {
        int i = 0; tok.type = T_ID;
        while ((*line_p >= 'a' && *line_p <= 'z') || (*line_p >= 'A' && *line_p <= 'Z') ||
               (*line_p >= '0' && *line_p <= '9') || *line_p == '_') {
            if (i < 63) tok.text[i++] = *line_p;
            line_p++;
        }
        tok.text[i] = 0;
        return;
    }
    tok.type = T_EOF;
}

static int parse_primary(void) {
    if (tok.type == T_NUM) { int v = tok.ival; lex_next(); return v; }
    if (tok.type == T_MINUS) { lex_next(); return -parse_primary(); }
    if (tok.type == T_LPAREN) { lex_next(); int v = parse_expr(); if (tok.type == T_RPAREN) lex_next(); return v; }
    if (tok.type == T_ID) { int v = get_var(tok.text); lex_next(); return v; }
    if (tok.type == T_STR) { int v = 0; lex_next(); return v; }
    return 0;
}

static int parse_mul(void) {
    int v = parse_primary();
    while (tok.type == T_STAR || tok.type == T_SLASH || tok.type == T_PERCENT) {
        int op = tok.type; lex_next();
        int r = parse_primary();
        if (op == T_STAR) v *= r;
        else if (op == T_SLASH && r != 0) v /= r;
        else if (op == T_PERCENT && r != 0) v %= r;
    }
    return v;
}

static int parse_expr(void) {
    int v = parse_mul();
    while (tok.type == T_PLUS || tok.type == T_MINUS) {
        int op = tok.type; lex_next();
        int r = parse_mul();
        if (op == T_PLUS) v += r;
        else v -= r;
    }
    return v;
}

static int parse_cmp(void) {
    int v = parse_expr();
    if (tok.type == T_EQEQ || tok.type == T_NE || tok.type == T_LT ||
        tok.type == T_GT || tok.type == T_LE || tok.type == T_GE) {
        int op = tok.type; lex_next();
        int r = parse_expr();
        if (op == T_EQEQ) return v == r;
        if (op == T_NE) return v != r;
        if (op == T_LT) return v < r;
        if (op == T_GT) return v > r;
        if (op == T_LE) return v <= r;
        if (op == T_GE) return v >= r;
    }
    return v;
}

// --- Line-based interpreter ---

static const char *src;
static int line_num;

#define MAX_LINE 256
static char line_buf[MAX_LINE];

static int read_line(void) {
    for (;;) {
        if (*src == 0) return -1;
        while (*src == '\n') { src++; line_num++; }
        if (*src == 0) return -1;
        int indent = 0;
        while (*src == ' ') { indent++; src++; }
        if (*src == '#') {
            while (*src && *src != '\n') src++;
            if (*src == '\n') { src++; line_num++; }
            continue;
        }
        if (*src == '\n') { src++; line_num++; continue; }
        if (*src == 0) return -1;
        int i = 0;
        while (*src && *src != '\n' && i < MAX_LINE - 1) line_buf[i++] = *src++;
        line_buf[i] = 0;
        if (*src == '\n') src++;
        line_num++;
        return indent;
    }
}

static char while_cond_text[MAX_LINE];

static void exec_line(const char *line, int indent);

static void exec_block(int parent_indent, int type, int cond, const char *while_line) {
    const char *body_start = src;
    int body_start_line = line_num;
    int body_set = 0;

    for (;;) {
        const char *saved = src;
        int saved_ln = line_num;
        int indent = read_line();
        if (indent < 0) break;

        if (!body_set) { body_set = 1; }

        if (indent <= parent_indent) {
            src = saved; line_num = saved_ln;
            break;
        }

        if (cond) exec_line(line_buf, indent);
    }

    if (type == 1 && while_line) {
        line_p = while_line;
        lex_next();
        lex_next();
        int new_cond = parse_cmp();
        if (tok.type == T_COLON) lex_next();

        if (new_cond) {
            src = body_start;
            line_num = body_start_line;
            exec_block(parent_indent, type, new_cond, while_line);
        }
    }
}

static int kw_match(const char *kw) {
    const char *t = tok.text;
    while (*kw && *t && *kw == *t) { kw++; t++; }
    return *kw == 0 && (*t == 0 || *t == ' ' || *t == ':');
}

static int line_is_else(void) {
    const char *p = line_buf;
    while (*p == ' ') p++;
    return (p[0] == 'e' && p[1] == 'l' && p[2] == 's' && p[3] == 'e' &&
            (p[4] == ':' || p[4] == 0 || p[4] == ' '));
}

static int line_is_elif(void) {
    const char *p = line_buf;
    while (*p == ' ') p++;
    return (p[0] == 'e' && p[1] == 'l' && p[2] == 'i' && p[3] == 'f' &&
            (p[4] == ' ' || p[4] == ':'));
}

static void print_int(int v) {
    if (v < 0) { vga_putchar('-'); v = -v; }
    int d = 1, t = v;
    while (t >= 10) { d *= 10; t /= 10; }
    do { vga_putchar('0' + v / d); v %= d; d /= 10; } while (d);
}

static void exec_line(const char *line, int indent) {
    line_p = line;
    lex_next();
    if (tok.type == T_EOF) return;

    if (tok.type == T_ID && kw_match("print")) {
        lex_next();
        if (tok.type == T_LPAREN) lex_next();
        if (tok.type == T_STR) { vga_writestring(tok.text); lex_next(); }
        else { print_int(parse_expr()); }
        while (tok.type == T_RPAREN || tok.type == T_COMMA) lex_next();
        vga_putchar('\n');
        return;
    }

    if (tok.type == T_ID) {
        char name[32]; int ni = 0;
        while (tok.text[ni] && ni < 31) { name[ni] = tok.text[ni]; ni++; }
        name[ni] = 0;
        lex_next();
        if (tok.type == T_EQ) {
            lex_next();
            int v = parse_expr();
            set_var(name, v);
            return;
        }
        return;
    }

    if (tok.type == T_ID) {
        int is_if = kw_match("if");
        int is_while = kw_match("while");
        int is_elif = kw_match("elif");

        if (is_if || is_while || is_elif) {
            lex_next();
            int cond = parse_cmp();
            if (tok.type == T_COLON) lex_next();

            if (is_while) {
                int i = 0;
                while (line[i]) { while_cond_text[i] = line[i]; i++; }
                while_cond_text[i] = 0;
                exec_block(indent, 1, cond, while_cond_text);
            } else {
                exec_block(indent, 0, cond, NULL);

                if (cond == 0 && is_if) {
                    const char *save_src = src;
                    int save_ln = line_num;
                    int ei = read_line();
                    while (ei >= 0 && (ei > indent || (ei == indent && (line_is_elif() || line_is_else())))) {
                        if (ei == indent && line_is_elif()) {
                            line_p = line_buf;
                            lex_next(); // skip "elif"
                            int elif_cond = parse_cmp();
                            if (tok.type == T_COLON) lex_next();
                            exec_block(indent, 0, elif_cond, NULL);
                            if (elif_cond) break;
                            save_src = src; save_ln = line_num;
                            ei = read_line();
                            continue;
                        }
                        if (ei == indent && line_is_else()) {
                            exec_block(indent, 0, 1, NULL);
                            save_src = src; save_ln = line_num;
                            break;
                        }
                        ei = read_line();
                    }
                    src = save_src; line_num = save_ln;
                }
            }
            return;
        }
    }
}

int py_run(const char *source) {
    src = source;
    line_num = 1;
    var_n = 0;
    set_var("True", 1);
    set_var("False", 0);

    int indent;
    while ((indent = read_line()) >= 0) {
        exec_line(line_buf, indent);
    }
    return 1;
}

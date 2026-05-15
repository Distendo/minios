#include "asm.h"
#include "fs.h"
#include <stdint.h>

// registers
enum { REG_EAX, REG_ECX, REG_EDX, REG_EBX, REG_ESP, REG_EBP, REG_ESI, REG_EDI };
static const char *regs[] = {"eax","ecx","edx","ebx","esp","ebp","esi","edi",
                             "ax","cx","dx","bx","sp","bp","si","di",
                             "al","cl","dl","bl","ah","ch","dh","bh",0};

static int find_reg(const char *s) {
    for (int i = 0; regs[i]; i++) {
        int j = 0, match = 1;
        while (s[j] && regs[i][j] && s[j] == regs[i][j]) j++;
        if (regs[i][j] == 0 && (s[j] == 0 || s[j] == ' ' || s[j] == ',')) return i;
    }
    return -1;
}

static int is32reg(int r) {
    return r >= REG_EAX && r <= REG_EDI;
}

static int reg32to8(int r) {
    if (r >= 8 && r <= 15) return -1; // 16-bit
    if (r >= 16 && r <= 19) return r - 16; // al,cl,dl,bl
    if (r >= 20 && r <= 23) return r - 12; // ah,ch,dh,bh
    return -1;
}

static int is8reg(int r) {
    return r >= 16 && r <= 23;
}

// tokens
enum { T_EOF, T_ID, T_NUM, T_COLON, T_COMMA, T_LBRACK, T_RBRACK, T_PLUS, T_MINUS,
       T_NEWLINE, T_STRING };

typedef struct { int type; char text[64]; int val; } token_t;

static const char *srcp;
static int line_num;
static token_t cur;
static token_t peek;
static int have_peek;

// labels
#define MAX_LABELS 256
typedef struct { char name[64]; int addr; int pass; } label_t;
static label_t labels[MAX_LABELS];
static int num_labels;

// fixups (for two-pass)
#define MAX_FIXUPS 512
typedef struct { int offset; int bytes; char label[64]; int line; int instr_addr; } fixup_t;
static fixup_t fixups[MAX_FIXUPS];
static int num_fixups;

// output
static uint8_t *output;
static int out_pos;
static int out_max;
static int pass;

static void emit(uint8_t b) {
    if (out_pos < out_max) output[out_pos] = b;
    out_pos++;
}

static void emit32(uint32_t v) {
    emit(v & 0xFF); emit((v >> 8) & 0xFF);
    emit((v >> 16) & 0xFF); emit((v >> 24) & 0xFF);
}

static void emit16(uint16_t v) {
    emit(v & 0xFF); emit((v >> 8) & 0xFF);
}

static void error(const char *msg) {
    // silently record - output will contain error indicator
    output[0] = 0xFF;
    output[1] = 0xFF;
    output[2] = 0xFF;
    out_pos = 3;
}

static int is_id_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '.';
}

static void next_tok(void) {
    if (have_peek) { cur = peek; have_peek = 0; return; }
    while (*srcp == ' ' || *srcp == '\t') srcp++;
    if (*srcp == 0) { cur.type = T_EOF; return; }
    if (*srcp == ';') {
        while (*srcp && *srcp != '\n') srcp++;
        if (*srcp == '\n') { srcp++; line_num++; }
        while (*srcp == ' ' || *srcp == '\t') srcp++;
        if (*srcp == 0) { cur.type = T_EOF; return; }
    }
    if (*srcp == '\n') { srcp++; line_num++; cur.type = T_NEWLINE; return; }
    if (*srcp == ':') { srcp++; cur.type = T_COLON; return; }
    if (*srcp == ',') { srcp++; cur.type = T_COMMA; return; }
    if (*srcp == '[') { srcp++; cur.type = T_LBRACK; return; }
    if (*srcp == ']') { srcp++; cur.type = T_RBRACK; return; }
    if (*srcp == '+') { srcp++; cur.type = T_PLUS; return; }
    if (*srcp == '-') { srcp++; cur.type = T_MINUS; return; }
    if (*srcp == '\'') {
        srcp++;
        cur.type = T_NUM;
        cur.val = *srcp;
        if (*srcp) srcp++;
        if (*srcp == '\'') srcp++;
        return;
    }
    if (*srcp == '$') {
        srcp++;
        cur.type = T_NUM; cur.val = 0;
        while ((*srcp >= '0' && *srcp <= '9') || (*srcp >= 'a' && *srcp <= 'f') ||
               (*srcp >= 'A' && *srcp <= 'F')) {
            cur.val = cur.val * 16 + (*srcp <= '9' ? *srcp - '0' : (*srcp | 32) - 'a' + 10);
            srcp++;
        }
        return;
    }
    if (*srcp >= '0' && *srcp <= '9') {
        cur.type = T_NUM; cur.val = 0;
        if (*srcp == '0' && (*(srcp+1) == 'x' || *(srcp+1) == 'X')) {
            srcp += 2;
            while ((*srcp >= '0' && *srcp <= '9') || (*srcp >= 'a' && *srcp <= 'f') ||
                   (*srcp >= 'A' && *srcp <= 'F')) {
                cur.val = cur.val * 16 + (*srcp <= '9' ? *srcp - '0' : (*srcp | 32) - 'a' + 10);
                srcp++;
            }
        } else {
            while (*srcp >= '0' && *srcp <= '9') {
                cur.val = cur.val * 10 + (*srcp - '0');
                srcp++;
            }
        }
        return;
    }
    if (is_id_char(*srcp)) {
        int i = 0;
        cur.type = T_ID;
        while (is_id_char(*srcp) && i < 63) cur.text[i++] = *srcp++;
        cur.text[i] = 0;
        return;
    }
    cur.type = T_EOF;
}

static void unget(void) {
    peek = cur; have_peek = 1;
}

static int expect(int type) {
    if (cur.type != type) { error("syntax"); return 0; }
    next_tok();
    return 1;
}

static int read_expr(void) {
    int v = 0, sign = 1;
    if (cur.type == T_PLUS) { next_tok(); }
    else if (cur.type == T_MINUS) { sign = -1; next_tok(); }
    if (cur.type == T_NUM) { v = cur.val; next_tok(); return v * sign; }
    // could be a label reference
    if (cur.type == T_ID) {
        // look up label
        for (int i = 0; i < num_labels; i++)
            if (labels[i].name[0] == cur.text[0]) {
                int j = 0, match = 1;
                while (labels[i].name[j] && cur.text[j] && labels[i].name[j] == cur.text[j]) j++;
                if (labels[i].name[j] == 0 && cur.text[j] == 0) {
                    next_tok();
                    if (pass == 2) return labels[i].addr;
                    return 0; // first pass, return 0 as placeholder
                }
            }
        // unknown label - could be forward ref
        next_tok();
        return 0;
    }
    return v * sign;
}

static int find_label(const char *name) {
    for (int i = 0; i < num_labels; i++) {
        int j = 0, match = 1;
        while (labels[i].name[j] && name[j] && labels[i].name[j] == name[j]) j++;
        if (labels[i].name[j] == 0 && name[j] == 0) return i;
    }
    return -1;
}

static int add_fixup(int offset, int bytes, const char *label, int instr_addr) {
    if (num_fixups >= MAX_FIXUPS) return 0;
    fixups[num_fixups].offset = offset;
    fixups[num_fixups].bytes = bytes;
    int i;
    for (i = 0; label[i] && i < 63; i++) fixups[num_fixups].label[i] = label[i];
    fixups[num_fixups].label[i] = 0;
    fixups[num_fixups].line = line_num;
    fixups[num_fixups].instr_addr = instr_addr;
    num_fixups++;
    return 1;
}

static void resolve_fixups(void) {
    for (int f = 0; f < num_fixups; f++) {
        int li = find_label(fixups[f].label);
        if (li < 0) { /* unresolved - output will be 0 */ continue; }
        int target = labels[li].addr;
        int off = fixups[f].offset;
        if (fixups[f].bytes == 1) {
            int disp = target - (fixups[f].instr_addr + 2); // short jump: 2 bytes total
            output[off] = disp & 0xFF;
        } else if (fixups[f].bytes == 2) {
            int disp = target - (fixups[f].instr_addr + 6); // near conditional: 6 bytes
            output[off] = disp & 0xFF;
            output[off + 1] = (disp >> 8) & 0xFF;
        } else if (fixups[f].bytes == 4) {
            output[off] = target & 0xFF;
            output[off + 1] = (target >> 8) & 0xFF;
            output[off + 2] = (target >> 16) & 0xFF;
            output[off + 3] = (target >> 24) & 0xFF;
        }
    }
}

int asm_assemble(const char *src, uint8_t *out, int max_out, const char *outname) {
    output = out;
    out_max = max_out;
    
    // Two-pass assembly
    for (pass = 1; pass <= 2; pass++) {
        srcp = src;
        line_num = 1;
        num_labels = 0;
        num_fixups = 0;
        have_peek = 0;
        out_pos = 0;
        
        next_tok();
        while (cur.type != T_EOF) {
            if (cur.type == T_NEWLINE) { next_tok(); continue; }
            
            if (cur.type == T_ID) {
                char id[64];
                int i;
                for (i = 0; cur.text[i] && i < 63; i++) id[i] = cur.text[i];
                id[i] = 0;
                next_tok();
                
                // Check for label
                if (cur.type == T_COLON) {
                    next_tok();
                    if (pass == 1) {
                        if (find_label(id) < 0) {
                            int li;
                            for (li = 0; id[li] && li < 63; li++) labels[num_labels].name[li] = id[li];
                            labels[num_labels].name[li] = 0;
                            labels[num_labels].addr = out_pos;
                            labels[num_labels].pass = 1;
                            num_labels++;
                        }
                    } else {
                        int li = find_label(id);
                        if (li >= 0) labels[li].addr = out_pos;
                    }
                    if (cur.type == T_NEWLINE) { next_tok(); continue; }
                    if (cur.type == T_EOF) break;
                }
                
                // Directives
                if (id[0] == 'd' && id[1] == 'b' && id[2] == 0) {
                    while (cur.type != T_NEWLINE && cur.type != T_EOF) {
                        if (cur.type == T_COMMA) { next_tok(); continue; }
                        int v = read_expr();
                        emit(v & 0xFF);
                    }
                    if (cur.type == T_NEWLINE) next_tok();
                    continue;
                }
                if (id[0] == 'd' && id[1] == 'w' && id[2] == 0) {
                    while (cur.type != T_NEWLINE && cur.type != T_EOF) {
                        if (cur.type == T_COMMA) { next_tok(); continue; }
                        // could be a label reference
                        if (cur.type == T_ID) {
                            int li = find_label(cur.text);
                            if (li >= 0) {
                                emit16(labels[li].addr);
                                next_tok();
                            } else {
                                if (pass == 1) {
                                    emit16(0);
                                    next_tok();
                                } else {
                                    // forward reference
                                    int sav = out_pos;
                                    emit16(0);
                                    add_fixup(sav - 2, 2, cur.text, 0);
                                    next_tok();
                                }
                            }
                        } else {
                            int v = read_expr();
                            emit16(v & 0xFFFF);
                        }
                    }
                    if (cur.type == T_NEWLINE) next_tok();
                    continue;
                }
                if (id[0] == 'r' && id[1] == 'e' && id[2] == 's' && id[3] == 'b' && id[4] == 0) {
                    int v = read_expr();
                    for (int j = 0; j < v; j++) emit(0);
                    if (cur.type == T_NEWLINE) next_tok();
                    continue;
                }
                
                // --- Instructions ---
                int r1, r2, v;
                int sav_pos = out_pos;
                
                // mov reg, imm/reg/[addr]  or  mov [addr], reg
                if (id[0] == 'm' && id[1] == 'o' && id[2] == 'v' && id[3] == 0) {
                    if (cur.type == T_LBRACK) {
                        // mov [addr], reg
                        next_tok();
                        char lb[64]; int is_num = 0; int num_val = 0;
                        if (cur.type == T_NUM) { num_val = read_expr(); is_num = 1; }
                        else { int li; for (li = 0; cur.text[li] && li < 63; li++) lb[li] = cur.text[li]; lb[li] = 0; next_tok(); }
                        expect(T_RBRACK);
                        expect(T_COMMA);
                        r2 = find_reg(cur.text); next_tok();
                        if (r2 >= REG_EAX && r2 <= REG_EDI) {
                            if (r2 == REG_EAX) {
                                emit(0xA3);
                                if (is_num) emit32(num_val);
                                else { emit32(0); add_fixup(out_pos - 4, 4, lb, out_pos - 5); }
                            } else {
                                emit(0x89); emit(0x05 + r2 * 8);
                                if (is_num) emit32(num_val);
                                else { emit32(0); add_fixup(out_pos - 4, 4, lb, out_pos - 6); }
                            }
                        }
                    } else {
                        r1 = find_reg(cur.text);
                        next_tok();
                        expect(T_COMMA);
                        if (cur.type == T_LBRACK) {
                            // mov reg, [addr]
                            next_tok();
                            char lb[64]; int is_num = 0; int num_val = 0;
                            if (cur.type == T_NUM) { num_val = read_expr(); is_num = 1; }
                            else { int li; for (li = 0; cur.text[li] && li < 63; li++) lb[li] = cur.text[li]; lb[li] = 0; next_tok(); }
                            expect(T_RBRACK);
                            if (r1 >= REG_EAX && r1 <= REG_EDI) {
                                if (r1 == REG_EAX) {
                                    emit(0xA1);
                                    if (is_num) emit32(num_val);
                                    else { emit32(0); add_fixup(out_pos - 4, 4, lb, out_pos - 5); }
                                } else {
                                    emit(0x8B); emit(0x05 + r1 * 8);
                                    if (is_num) emit32(num_val);
                                    else { emit32(0); add_fixup(out_pos - 4, 4, lb, out_pos - 6); }
                                }
                            }
                        } else if (cur.type == T_NUM) {
                            v = read_expr();
                            if (r1 >= REG_EAX && r1 <= REG_EDI) {
                                emit(0xB8 + r1); emit32(v);
                            } else if (r1 >= 8 && r1 <= 15) {
                                emit(0x66); emit(0xB8 + (r1 - 8)); emit16(v);
                            }
                        } else if (cur.type == T_ID && (r2 = find_reg(cur.text)) >= 0) {
                            next_tok();
                            if (r1 >= REG_EAX && r1 <= REG_EDI && r2 >= REG_EAX && r2 <= REG_EDI) {
                                emit(0x89); emit(0xC0 + r2 * 8 + r1);
                            }
                        }
                    }
                    if (cur.type == T_NEWLINE) next_tok();
                    continue;
                }
                
                // add/sub/cmp reg, imm/reg
                if ((id[0] == 'a' && id[1] == 'd' && id[2] == 'd' && id[3] == 0) ||
                    (id[0] == 's' && id[1] == 'u' && id[2] == 'b' && id[3] == 0) ||
                    (id[0] == 'c' && id[1] == 'm' && id[2] == 'p' && id[3] == 0)) {
                    uint8_t op = (id[0] == 'a') ? 0x01 : (id[0] == 's') ? 0x29 : 0x39;
                    uint8_t imm_op = (id[0] == 'a') ? 0x81 : (id[0] == 's') ? 0x81 : 0x83;
                    uint8_t imm_reg = (id[0] == 'a') ? 0 : (id[0] == 's') ? 5 : 7;
                    r1 = find_reg(cur.text); next_tok();
                    expect(T_COMMA);
                    if (cur.type == T_NUM) {
                        v = read_expr();
                        if (v >= -128 && v <= 127) {
                            emit(0x83); emit(0xC0 + imm_reg * 8 + r1); emit(v & 0xFF);
                        } else {
                            emit(0x81); emit(0xC0 + imm_reg * 8 + r1); emit32(v);
                        }
                    } else if ((r2 = find_reg(cur.text)) >= 0) {
                        next_tok();
                        emit(op); emit(0xC0 + r2 * 8 + r1);
                    }
                    if (cur.type == T_NEWLINE) next_tok();
                    continue;
                }
                
                // imul
                if (id[0] == 'i' && id[1] == 'm' && id[2] == 'u' && id[3] == 'l' && id[4] == 0) {
                    r1 = find_reg(cur.text); next_tok();
                    expect(T_COMMA); r2 = find_reg(cur.text); next_tok();
                    expect(T_COMMA); v = read_expr();
                    emit(0x69); emit(0xC0 + r2 * 8 + r1); emit32(v);
                    if (cur.type == T_NEWLINE) next_tok();
                    continue;
                }
                
                // idiv
                if (id[0] == 'i' && id[1] == 'd' && id[2] == 'i' && id[3] == 'v' && id[4] == 0) {
                    r1 = find_reg(cur.text); next_tok();
                    emit(0xF7); emit(0xF8 + r1);
                    if (cur.type == T_NEWLINE) next_tok();
                    continue;
                }
                
                // inc/dec
                if ((id[0] == 'i' && id[1] == 'n' && id[2] == 'c' && id[3] == 0) ||
                    (id[0] == 'd' && id[1] == 'e' && id[2] == 'c' && id[3] == 0)) {
                    r1 = find_reg(cur.text); next_tok();
                    if (id[0] == 'i') emit(0x40 + r1); else emit(0x48 + r1);
                    if (cur.type == T_NEWLINE) next_tok();
                    continue;
                }
                
                // push/pop
                if ((id[0] == 'p' && id[1] == 'u' && id[2] == 's' && id[3] == 'h' && id[4] == 0) ||
                    (id[0] == 'p' && id[1] == 'o' && id[2] == 'p' && id[3] == 0)) {
                    r1 = find_reg(cur.text); next_tok();
                    if (id[0] == 'p' && id[1] == 'u') emit(0x50 + r1);
                    else emit(0x58 + r1);
                    if (cur.type == T_NEWLINE) next_tok();
                    continue;
                }
                
                // ret/nop/hlt/int
                if (id[0] == 'r' && id[1] == 'e' && id[2] == 't' && id[3] == 0) {
                    emit(0xC3);
                    if (cur.type == T_NEWLINE) next_tok();
                    continue;
                }
                if (id[0] == 'n' && id[1] == 'o' && id[2] == 'p' && id[3] == 0) {
                    emit(0x90);
                    if (cur.type == T_NEWLINE) next_tok();
                    continue;
                }
                if (id[0] == 'h' && id[1] == 'l' && id[2] == 't' && id[3] == 0) {
                    emit(0xF4);
                    if (cur.type == T_NEWLINE) next_tok();
                    continue;
                }
                if (id[0] == 'i' && id[1] == 'n' && id[2] == 't' && id[3] == 0) {
                    next_tok(); v = read_expr();
                    emit(0xCD); emit(v & 0xFF);
                    if (cur.type == T_NEWLINE) next_tok();
                    continue;
                }
                
                // xchg
                if (id[0] == 'x' && id[1] == 'c' && id[2] == 'h' && id[3] == 'g' && id[4] == 0) {
                    r1 = find_reg(cur.text); next_tok();
                    expect(T_COMMA);
                    if ((r2 = find_reg(cur.text)) >= 0) { next_tok(); emit(0x87); emit(0xC0 + r2 * 8 + r1); }
                    if (cur.type == T_NEWLINE) next_tok();
                    continue;
                }
                
                // mov reg, [expr]
                if (id[0] == 'm' && id[1] == 'o' && id[2] == 'v' && id[3] == 0 && cur.type == T_LBRACK) {
                    // re-read, already processed as regular mov above, this is fallback
                    // Actually handle properly: we already consumed the first reg
                    // Need to handle mov reg, [addr]
                    // This is a simplified version - skip for now
                    // Just emit placeholder
                    emit(0xA1); emit32(0); // mov eax, [addr]
                    while (cur.type != T_NEWLINE && cur.type != T_EOF) next_tok();
                    if (cur.type == T_NEWLINE) next_tok();
                    continue;
                }
                
                // jmp/call to label
                if ((id[0] == 'j' && id[1] == 'm' && id[2] == 'p' && id[3] == 0) ||
                    (id[0] == 'c' && id[1] == 'a' && id[2] == 'l' && id[3] == 'l' && id[4] == 0)) {
                    char lb[64];
                    for (i = 0; cur.text[i] && i < 63; i++) lb[i] = cur.text[i];
                    lb[i] = 0;
                    next_tok();
                    int is_call = (id[0] == 'c');
                    
                    if (pass == 1) {
                        emit(0xE8 + (is_call ? 0 : 1)); emit32(0);
                    } else {
                        int li = find_label(lb);
                        int instr_start = sav_pos;
                        int disp;
                        if (is_call) {
                            emit(0xE8);
                            if (li >= 0) disp = labels[li].addr - (sav_pos + 5);
                            else disp = 0;
                            emit32(disp);
                            if (li < 0) add_fixup(sav_pos + 1, 4, lb, sav_pos);
                        } else {
                            emit(0xE9);
                            if (li >= 0) disp = labels[li].addr - (sav_pos + 5);
                            else disp = 0;
                            emit32(disp);
                            if (li < 0) add_fixup(sav_pos + 1, 4, lb, sav_pos);
                        }
                    }
                    if (cur.type == T_NEWLINE) next_tok();
                    continue;
                }
                
                // Conditional jumps: je/jne/jl/jg/jle/jge
                if (id[0] == 'j' && id[1] != 0 && id[2] != 0) {
                    uint8_t near_op = 0;
                    uint8_t short_op = 0;
                    char cond = id[1];
                    char cond2 = id[2];
                    if (cond == 'e' && cond2 == 0) { near_op = 0x84; short_op = 0x74; }
                    else if (cond == 'n' && id[2] == 'e' && id[3] == 0) { near_op = 0x85; short_op = 0x75; }
                    else if (cond == 'l' && cond2 == 0) { near_op = 0x8C; short_op = 0x7C; }
                    else if (cond == 'g' && cond2 == 0) { near_op = 0x8F; short_op = 0x7F; }
                    else if (cond == 'l' && id[2] == 'e' && id[3] == 0) { near_op = 0x8E; short_op = 0x7E; }
                    else if (cond == 'g' && id[2] == 'e' && id[3] == 0) { near_op = 0x8D; short_op = 0x7D; }
                    else { /* unrecognized */ if (cur.type == T_NEWLINE) next_tok(); continue; }
                    
                    char lb[64];
                    for (i = 0; cur.text[i] && i < 63; i++) lb[i] = cur.text[i];
                    lb[i] = 0;
                    next_tok();
                    
                    if (pass == 1) {
                        emit(0x0F); emit(near_op); emit32(0);
                    } else {
                        int li = find_label(lb);
                        int instr_start = sav_pos;
                        if (li >= 0) {
                            int disp = labels[li].addr - (instr_start + 6);
                            emit(0x0F); emit(near_op); emit32(disp);
                        } else {
                            emit(0x0F); emit(near_op); emit32(0);
                            add_fixup(sav_pos + 2, 4, lb, sav_pos);
                        }
                    }
                    if (cur.type == T_NEWLINE) next_tok();
                    continue;
                }
                
                // Unknown instruction - skip line
                while (cur.type != T_NEWLINE && cur.type != T_EOF) next_tok();
                if (cur.type == T_NEWLINE) next_tok();
            } else {
                next_tok();
            }
        }
        
        // On pass 2, resolve fixups and write output
        if (pass == 2) {
            resolve_fixups();
            // Write output to file
            if (outname && outname[0]) {
                fs_write(outname, output, out_pos);
            }
        } else if (pass == 1) {
            // After pass 1, labels are resolved, rewind output
            // (output already read positions)
        }
    }
    
    return out_pos;
}
#ifndef ASM_H
#define ASM_H

#include <stdint.h>

int asm_assemble(const char *src, uint8_t *out, int max_out, const char *outname);

#endif
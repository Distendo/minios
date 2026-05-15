#ifndef TINYCC_H
#define TINYCC_H

#include <stdint.h>

int tcc_compile(const char *source, char *asm_out, int max_out);
int tcc_build(const char *source, const char *outname);

#endif
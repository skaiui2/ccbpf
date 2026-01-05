#ifndef BPF_EMIT_H
#define BPF_EMIT_H

#include <stdarg.h>


void bpf_on_emit(const char *fmt, va_list ap);
void bpf_mark_label(int label);
struct bpf_insn *bpf_get_program(int *out_count);


#endif
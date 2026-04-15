#ifndef CCBPF_H
#define CCBPF_H

#include <stdint.h>
#include "hashmap.h"
#include "cbpf.h"

extern struct hashmap native_table;

typedef uint32_t (*native_fn_t)(struct ccbpf_program *p,
                                uint32_t a0,
                                uint32_t a1,
                                uint32_t a2,
                                uint32_t a3);


struct native_entry {
    int         func_id;
    int         argc;
    native_fn_t fn;
};


void ccbpf_system_init(void);
void hook_register(const char *name);
void native_register(int func_id, int argc, native_fn_t fn);
uint32_t ccbpf_run_frame(struct ccbpf_program *p,
                         void *frame,
                         size_t frame_size);

struct ccbpf_program *ccbpf_load(const char *path);
int hook_attach(const char *hook_name, uint8_t *image, size_t len);
int hook_detach(const char *hook_name);
uint32_t hook_run(const char *hook_name, uint8_t *frame, size_t frame_size);
struct ccbpf_program *ccbpf_load_from_memory(const uint8_t *image, size_t len);
void ccbpf_unload(struct ccbpf_program *p);
uint32_t ccbpf_run(struct ccbpf_program *p);

#endif

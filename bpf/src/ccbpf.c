#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "ccbpf.h" 

struct ccbpf_program *ccbpf_load(const char *path)
{
    struct ccbpf_program *prog = calloc(1, sizeof(*prog));
    if (!prog)
        return NULL;

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        perror("ccbpf_load fopen");
        free(prog);
        return NULL;
    }

    struct CCBPF_Header hdr;
    fread(&hdr, sizeof(hdr), 1, fp);

    if (hdr.magic != CCBPF_MAGIC) {
        fprintf(stderr, "Invalid CCBPF magic\n");
        fclose(fp);
        free(prog);
        return NULL;
    }

    // get code
    fseek(fp, hdr.code_offset, SEEK_SET);
    size_t insn_count = hdr.code_size / sizeof(struct bpf_insn);

    prog->insns = malloc(hdr.code_size);
    fread(prog->insns, sizeof(struct bpf_insn), insn_count, fp);
    prog->insn_count = insn_count;

    // get string (in data area)
    prog->string_count = 0;
    prog->strings      = NULL;

    if (hdr.data_size > 0) {
        fseek(fp, hdr.data_offset, SEEK_SET);

        int count = 0;
        fread(&count, sizeof(int), 1, fp);
        prog->string_count = count;

        prog->strings = calloc(count, sizeof(char *));
        for (int i = 0; i < count; i++) {
            int len = 0;
            fread(&len, sizeof(int), 1, fp);

            char *buf = malloc(len);
            fread(buf, 1, len, fp);

            prog->strings[i] = buf;
        }
    }

    fclose(fp);

    prog->entry = hdr.entry;

    prog->map_count = CCBPF_MAX_MAPS;
    for (size_t i = 0; i < prog->map_count; i++) {
        hashmap_init(&prog->maps[i], 64, HASHMAP_KEY_INT);
    }

    return prog;
}


void ccbpf_unload(struct ccbpf_program *prog)
{
    if (!prog)
        return;

    if (prog->insns)
        free(prog->insns);

    if (prog->data)
        free(prog->data);

    for (size_t i = 0; i < prog->map_count; i++) {
        hashmap_destroy(&prog->maps[i]);
    }

    free(prog);
}

uint32_t ccbpf_run_frame(struct ccbpf_program *prog,
                         void *frame,
                         size_t frame_size)
{
    return ccbpf_vm_exec(prog,
                         prog->insns,
                         (unsigned char *)frame,
                         frame_size,
                         frame_size);
}

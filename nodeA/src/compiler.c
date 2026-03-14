#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <limits.h>
#include "lexer.h"
#include "parser.h"
#include "ir.h"
#include "bpf_builder.h"
#include "cbpf.h"
#include "ir_lowering.h"
#include "compiler.h"
#include "heap.h"

#define HOOK_SOCK_PATH "/tmp/ccbpf_hook.sock"

static char *load_file(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    char *buf = malloc(size + 1);
    if (!buf) {
        fclose(fp);
        return NULL;
    }

    fread(buf, 1, size, fp);
    buf[size] = '\0';
    fclose(fp);
    return buf;
}

static int send_cmd(const char *cmd)
{
    int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) return 1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, HOOK_SOCK_PATH, sizeof(addr.sun_path) - 1);

    ssize_t n = sendto(fd, cmd, strlen(cmd), 0,
                       (struct sockaddr *)&addr, sizeof(addr));
    close(fd);
    return (n < 0);
}

static int compile_to_bpf(const char *src_path, const char *out_path)
{
    char *src = load_file(src_path);
    if (!src) {
        fprintf(stderr, "failed to load %s\n", src_path);
        return 1;
    }

    compiler_init(16, 20*1024, 10*1024);

    struct lexer lex;
    lexer_init(&lex);
    lexer_set_input_buffer(src, strlen(src));

    struct Parser *p = parser_new(&lex);
    parser_program(p);

    mg_region_print_pools(frontend_region);
    mg_region_print_pools(longterm_region);
    mg_region_print_pools(ir_region);
    frontend_destroy(&lex);

    heap_get_stats();
    struct bpf_builder b;
    bpf_builder_init(&b, 40*1024);

    heap_get_stats();

    struct ir_mes im;
    ir_mes_get(&im);

    ir_lower_program(im.ir_head, im.label_count, &b);

    struct bpf_insn *prog = bpf_builder_data(&b);
    int prog_len = bpf_builder_count(&b);

    size_t image_len = 0;
    uint8_t *image = ccbpf_pack_memory(prog, prog_len, &image_len);

    FILE *out = fopen(out_path, "wb");
    if (!out) {
        fprintf(stderr, "failed to open %s\n", out_path);
        free(src);
        return 1;
    }

    fwrite(image, 1, image_len, out);
    fclose(out);

    printf("Wrote %s (%zu bytes)\n", out_path, image_len);

    free(src);

    heap_get_stats();
    mg_region_print_pools(backend_region);
    bpf_builder_free(&b);
    heap_get_stats();
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage:\n");
        fprintf(stderr, "  %s <src.c> -o <out.ccbpf>\n", argv[0]);
        fprintf(stderr, "  %s attach <hook_name> <path-to-ccbpf>\n", argv[0]);
        fprintf(stderr, "  %s detach <hook_name>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "attach") != 0 &&
        strcmp(argv[1], "detach") != 0)
    {
        if (argc != 4 || strcmp(argv[2], "-o") != 0) {
            fprintf(stderr, "usage: %s <src.c> -o <out.ccbpf>\n", argv[0]);
            return 1;
        }

        return compile_to_bpf(argv[1], argv[3]);
    }

    if (strcmp(argv[1], "attach") == 0) {
        if (argc != 4) return 1;

        char abs_path[256];
        if (!realpath(argv[3], abs_path)) return 1;

        char buf[512];
        snprintf(buf, sizeof(buf), "ATTACH %s %s", argv[2], abs_path);
        return send_cmd(buf);
    }

    if (strcmp(argv[1], "detach") == 0) {
        if (argc != 3) return 1;

        char buf[512];
        snprintf(buf, sizeof(buf), "DETACH %s", argv[2]);
        return send_cmd(buf);
    }

    return 1;
}

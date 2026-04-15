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
#include "heap.h"

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

static int send_cmd_to(const char *sock, const char *cmd)
{
    int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) return 1;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock, sizeof(addr.sun_path) - 1);
    ssize_t n = sendto(fd, cmd, strlen(cmd), 0, (struct sockaddr *)&addr, sizeof(addr));
    close(fd);
    return (n < 0);
}

void native_init()
{
    native_decl_register("ntohl",       1, 1);
    native_decl_register("ntohs",       2, 1);
    native_decl_register("print",       3, 1);
    native_decl_register("print_str",   4, 1);
    native_decl_register("map_lookup",  5, 2);
    native_decl_register("map_update",  6, 3);
    native_decl_register("now_time",    7, 0);
    native_decl_register("access",      8, 3);
    native_decl_register("prints",      9, 1);
}

static int compile_to_bpf(const char *src_path, const char *out_path)
{
    char *src = load_file(src_path);
    if (!src) return 1;

    compiler_init(16, 30*1024, 1*1024, 15*1024);

    struct lexer lex;
    lexer_init(&lex);
    lexer_set_input_buffer(src, strlen(src));

    struct Parser *p = parser_new(&lex);

    native_init();
    parser_program(p);

    struct bpf_builder b;
    bpf_builder_init(&b, 24*1024);

    struct ir_mes im;
    ir_mes_get(&im);

    ir_lower_program(im.ir_head, im.label_count, &b);

    struct bpf_insn *prog = bpf_builder_data(&b);
    int prog_len = bpf_builder_count(&b);

    ir_free();
    size_t image_len = 0;
    uint8_t *image = ccbpf_pack_memory(prog, 7*1024, prog_len, &image_len);

    FILE *out = fopen(out_path, "wb");
    if (!out) {
        free(src);
        return 1;
    }

    fwrite(image, 1, image_len, out);
    fclose(out);
    free(src);
    bpf_builder_free(&b);
    return 0;
}

static void usage(const char *p)
{
    fprintf(stderr, "usage:\n");
    fprintf(stderr, "  %s <src.c> -o <out.ccbpf>\n", p);
    fprintf(stderr, "  %s attach <sock_path> <hook_name> <ccbpf>\n", p);
    fprintf(stderr, "  %s detach <sock_path> <hook_name>\n", p);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "attach") != 0 &&
        strcmp(argv[1], "detach") != 0)
    {
        if (argc != 4 || strcmp(argv[2], "-o") != 0) {
            usage(argv[0]);
            return 1;
        }
        return compile_to_bpf(argv[1], argv[3]);
    }

    if (strcmp(argv[1], "attach") == 0) {
        if (argc != 5) {
            usage(argv[0]);
            return 1;
        }
        char abs_path[256];
        if (!realpath(argv[4], abs_path)) return 1;
        char buf[512];
        snprintf(buf, sizeof(buf), "ATTACH %s %s", argv[3], abs_path);
        return send_cmd_to(argv[2], buf);
    }

    if (strcmp(argv[1], "detach") == 0) {
        if (argc != 4) {
            usage(argv[0]);
            return 1;
        }
        char buf[512];
        snprintf(buf, sizeof(buf), "DETACH %s", argv[3]);
        return send_cmd_to(argv[2], buf);
    }

    return 1;
}

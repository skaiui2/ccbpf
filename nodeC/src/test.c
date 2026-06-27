#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "lexer.h"
#include "parser.h"
#include "ir.h"
#include "bpf_builder.h"
#include "cbpf.h"
#include "ir_lowering.h"
#include "heap.h"
#include "ccbpf.h"

#define MIGRATE_SOCK_PATH "/tmp/ccbpf_migrate.sock"

static char *load_text_file_heap(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);
    char *buf = heap_malloc(size + 1);
    fread(buf, 1, size, fp);
    fclose(fp);
    buf[size] = 0;
    return buf;
}

static void native_init_frontend(void)
{
    native_decl_register("ntohl",      1, 1);
    native_decl_register("ntohs",      2, 1);
    native_decl_register("print",      3, 1);
    native_decl_register("print_str",  4, 1);
    native_decl_register("map_lookup", 5, 2);
    native_decl_register("map_update", 6, 3);
    native_decl_register("now_ms",     7, 0);
    native_decl_register("migrate",    8, 0);
}

uint32_t native_printf(struct ccbpf_program *p,
                       uint32_t a0,
                       uint32_t a1,
                       uint32_t a2,
                       uint32_t a3)
{
    printf("nodeC: %u\n", a0);
    sleep(1);
    return 0;
}

uint32_t native_print_str(struct ccbpf_program *p,
                          uint32_t a0,
                          uint32_t a1,
                          uint32_t a2,
                          uint32_t a3)
{
    if (a0 >= (uint32_t)p->string_count) return 0;
    printf("nodeC: %s\n", p->strings[a0]);
    sleep(1);
    return 0;
}

void native_register_all(void)
{
    native_register(3, 1, native_printf);
    native_register(4, 1, native_print_str);
    native_register(8, 0, native_migrate);
}

static uint8_t *compile_to_image(const char *src_path, size_t *out_len)
{
    char *src = load_text_file_heap(src_path);
    compiler_init(16, 30*1024, 1*1024, 15*1024);

    struct lexer lex;
    lexer_init(&lex);
    lexer_set_input_buffer(src, strlen(src));

    struct Parser *p = parser_new(&lex);
    native_init_frontend();
    parser_program(p);

    frontend_destroy(&lex);

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

    bpf_builder_free(&b);
    heap_free(src);

    *out_len = image_len;
    return image;
}

static void send_to_nodeD(uint8_t *img, size_t img_len,
                          uint8_t *ctx_buf, size_t ctx_len)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, MIGRATE_SOCK_PATH, sizeof(addr.sun_path)-1);

    connect(fd, (struct sockaddr *)&addr, sizeof(addr));

    write(fd, &img_len, sizeof(img_len));
    write(fd, img, img_len);
    write(fd, &ctx_len, sizeof(ctx_len));
    write(fd, ctx_buf, ctx_len);

    close(fd);
}

int main(int argc, char **argv)
{
    const char *src = argv[1];

    size_t img_len = 0;
    uint8_t *img = compile_to_image(src, &img_len);

    ccbpf_system_init();
    native_register_all();

    struct ccbpf_program *prog = ccbpf_load_from_memory(img, img_len);

    struct ccbpf_ctx ctx;
    memset(&ctx, 0, sizeof(ctx));

    unsigned char p[1] = {0};
    unsigned int wirelen = 1;
    unsigned int buflen  = 1;

    for (;;) {
        enum ccbpf_status st =
            ccbpf_vm_step(&ctx, prog, p, wirelen, buflen, 64);

        if (st == CCBPF_FINISHED) {
            printf("nodeC: finished %u\n", ctx.ret);
            heap_free(img);
            return 0;
        }

        if (st == CCBPF_MIGRATE) {
            printf("nodeC: migrate PC is %u\n", ctx.pc);
            break;
        }

        if (st == CCBPF_ERROR) {
            printf("nodeC: error\n");
            heap_free(img);
            return 1;
        }
    }

    uint8_t *ctx_buf = NULL;
    size_t ctx_len = 0;
    ccbpf_ctx_pack(&ctx, &ctx_buf, &ctx_len);

    send_to_nodeD(img, img_len, ctx_buf, ctx_len);

    heap_free(ctx_buf);
    heap_free(img);

    return 0;
}

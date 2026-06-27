#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "ccbpf.h"
#include "heap.h"

#define MIGRATE_SOCK_PATH "/tmp/ccbpf_migrate.sock"

static void recv_from_nodeC(uint8_t **img, size_t *img_len,
                            uint8_t **ctx_buf, size_t *ctx_len)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);

    struct sockaddr_un addr;
    unlink(MIGRATE_SOCK_PATH);
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, MIGRATE_SOCK_PATH, sizeof(addr.sun_path)-1);

    bind(fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(fd, 1);

    int cfd = accept(fd, NULL, NULL);

    read(cfd, img_len, sizeof(*img_len));
    *img = malloc(*img_len);
    read(cfd, *img, *img_len);

    read(cfd, ctx_len, sizeof(*ctx_len));
    *ctx_buf = malloc(*ctx_len);
    read(cfd, *ctx_buf, *ctx_len);

    close(cfd);
    close(fd);
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

int main(void)
{
    uint8_t *img = NULL;
    uint8_t *ctx_buf = NULL;
    size_t img_len = 0;
    size_t ctx_len = 0;

    recv_from_nodeC(&img, &img_len, &ctx_buf, &ctx_len);

    ccbpf_system_init();
    native_register_all();

    struct ccbpf_program *prog = ccbpf_load_from_memory(img, img_len);

    struct ccbpf_ctx ctx;
    ccbpf_ctx_unpack(&ctx, ctx_buf, ctx_len);

    free(ctx_buf);
    free(img);

    unsigned char p[1] = {0};
    unsigned int wirelen = 1;
    unsigned int buflen  = 1;

    for (;;) {
        enum ccbpf_status st =
            ccbpf_vm_step(&ctx, prog, p, wirelen, buflen, 64);

        if (st == CCBPF_FINISHED) {
            printf("nodeD: finished %u\n", ctx.ret);
            break;
        }

        if (st == CCBPF_ERROR) {
            printf("nodeD: error\n");
            break;
        }
    }

    return 0;
}
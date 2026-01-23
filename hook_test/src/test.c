#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>

#include "ccbpf.h"

#define HOOK_SOCK_PATH "/tmp/ccbpf_hook.sock"

struct hook_state {
    int attached;
    struct ccbpf_program prog;
};

static int g_hook_sock = -1;
static struct hook_state g_udp_input_hook = {0};

static int hook_control_init(void)
{
    int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, HOOK_SOCK_PATH, sizeof(addr.sun_path) - 1);

    unlink(HOOK_SOCK_PATH);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }

    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    g_hook_sock = fd;
    return 0;
}

static void hook_process_control_messages(void)
{
    if (g_hook_sock < 0)
        return;

    for (;;) {
        char buf[512];
        ssize_t n = recv(g_hook_sock, buf, sizeof(buf) - 1, 0);
        if (n <= 0)
            break;  

        buf[n] = '\0';

        if (strncmp(buf, "ATTACH ", 7) == 0) {
            char hook[64], path[256];
            if (sscanf(buf, "ATTACH %63s %255s", hook, path) != 2)
                continue;

            if (strcmp(hook, "hook_udp_input") != 0)
                continue;

            if (g_udp_input_hook.attached) {
                ccbpf_unload(&g_udp_input_hook.prog);
                g_udp_input_hook.attached = 0;
            }

            g_udp_input_hook.prog = ccbpf_load(path);
            g_udp_input_hook.attached = 1;

            printf("[hook] ATTACH hook_udp_input: %s\n", path);
        }

        else if (strncmp(buf, "DETACH ", 7) == 0) {
            char hook[64];
            if (sscanf(buf, "DETACH %63s", hook) != 1)
                continue;

            if (strcmp(hook, "hook_udp_input") != 0)
                continue;

            if (g_udp_input_hook.attached) {
                ccbpf_unload(&g_udp_input_hook.prog);
                g_udp_input_hook.attached = 0;
                printf("[hook] DETACH hook_udp_input\n");
            }
        }
    }
}

uint32_t hook_udp_input(uint8_t *frame, int frame_size)
{
    hook_process_control_messages();

    if (!g_udp_input_hook.attached)
        return 0;

    return ccbpf_run_frame(&g_udp_input_hook.prog, frame, frame_size);
}


struct udp_ctx {
    uint16_t sport;
    uint16_t dport;
};
    #include <arpa/inet.h>

int main(void)
{
    if (hook_control_init() < 0) {
        fprintf(stderr, "failed to init hook control socket\n");
        return 1;
    }

    uint8_t pkt_bytes[64];

    pkt_bytes[0] = 1;
    pkt_bytes[1] = 0;

    pkt_bytes[2] = 0;
    pkt_bytes[3] = 1;

    for (;;) {
        uint32_t r = hook_udp_input((uint8_t *)pkt_bytes, sizeof(pkt_bytes));
        printf("hook_udp_input() returned %u\n", r);
        sleep(1);
    }

    close(g_hook_sock);
    unlink(HOOK_SOCK_PATH);
    return 0;
}

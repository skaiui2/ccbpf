#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <time.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include "ccbpf.h"

#define HOOK_SOCK_PATH "/tmp/ccbpf_hook.sock"

struct udp_hdr {
    uint16_t sport;
    uint16_t dport;
    uint16_t len;
    uint16_t checksum;
};

struct hook_state {
    int attached;
    struct ccbpf_program *prog;
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
                ccbpf_unload(g_udp_input_hook.prog);
                g_udp_input_hook.prog = NULL;
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
                ccbpf_unload(g_udp_input_hook.prog);
                g_udp_input_hook.prog = NULL;
                g_udp_input_hook.attached = 0;
                printf("[hook] DETACH hook_udp_input\n");
            }
        }
    }
}

static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(1, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static uint64_t g_pkt_count = 0;
static uint64_t g_byte_count = 0;
static uint64_t g_last_ts = 0;

uint32_t hook_udp_input(uint8_t *frame, int frame_size)
{
    hook_process_control_messages();

    uint64_t now = now_ms();
    g_pkt_count++;
    g_byte_count += frame_size;

    if (now - g_last_ts >= 1000) {
        printf("[wirefisher] pps=%llu, bps=%llu\n",
               (unsigned long long)g_pkt_count,
               (unsigned long long)g_byte_count);
        g_pkt_count = 0;
        g_byte_count = 0;
        g_last_ts = now;
    }

    if (!g_udp_input_hook.attached || !g_udp_input_hook.prog)
        return 0;

    return ccbpf_run_frame(g_udp_input_hook.prog, frame, frame_size);
}


int main(void)
{
    if (hook_control_init() < 0) {
        fprintf(stderr, "failed to init hook control socket\n");
        return 1;
    }

    uint8_t pkt_bytes[4096];  
    struct udp_hdr *uh = (struct udp_hdr *)pkt_bytes;

    srand(time(NULL));

    for (;;) {
        int payload_len = 200;
        int udp_len = 8 + payload_len;

        uh->sport = htons(10000);
        uh->dport = htons(20000);
        uh->len   = htons(udp_len);
        uh->checksum = 0;

        for (int i = 8; i < udp_len; i++)
            pkt_bytes[i] = rand() & 0xFF;

        uint32_t r = hook_udp_input(pkt_bytes, udp_len);

        usleep(5000 + rand() % 45000);
    }

    close(g_hook_sock);
    unlink(HOOK_SOCK_PATH);
    return 0;
}

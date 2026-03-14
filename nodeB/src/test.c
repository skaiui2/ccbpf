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
#include "heap.h"

#define HOOK_SOCK_PATH "/tmp/ccbpf_hook.sock"

static int g_hook_sock = -1;

static int hook_control_init(void)
{
    int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, HOOK_SOCK_PATH, sizeof(addr.sun_path) - 1);

    unlink(HOOK_SOCK_PATH);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    g_hook_sock = fd;
    return 0;
}

static uint8_t *load_file(const char *path, size_t *len)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    uint8_t *buf = heap_malloc(size);
    if (!buf) {
        fclose(fp);
        return NULL;
    }

    fread(buf, 1, size, fp);
    fclose(fp);

    *len = size;
    return buf;
}

static void hook_process_control_messages(void)
{
    if (g_hook_sock < 0) return;

    for (;;) {
        char buf[512];
        ssize_t n = recv(g_hook_sock, buf, sizeof(buf) - 1, 0);
        if (n <= 0) break;

        buf[n] = '\0';

        if (strncmp(buf, "ATTACH ", 7) == 0) {
            char hook[64], path[256];
            if (sscanf(buf, "ATTACH %63s %255s", hook, path) != 2)
                continue;

            size_t len = 0;
            uint8_t *image = load_file(path, &len);
            if (!image) continue;

            hook_attach(hook, image, len);
            heap_free(image);
        }

        else if (strncmp(buf, "DETACH ", 7) == 0) {
            char hook[64];
            if (sscanf(buf, "DETACH %63s", hook) != 1)
                continue;

            hook_detach(hook);
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

int main(void)
{
    if (hook_control_init() < 0)
        return 1;

    uint8_t pkt_bytes[4096];
    struct udp_hdr {
        uint16_t sport;
        uint16_t dport;
        uint16_t len;
        uint16_t checksum;
    } *uh = (struct udp_hdr *)pkt_bytes;

    srand(time(NULL));

    for (;;) {
        hook_process_control_messages();

        int payload_len = 200;
        int udp_len = 8 + payload_len;

        uh->sport = htons(10000);
        uh->dport = htons(20000);
        uh->len   = htons(udp_len);
        uh->checksum = 0;

        for (int i = 8; i < udp_len; i++)
            pkt_bytes[i] = rand() & 0xFF;

        hook_run("hook_udp_input", pkt_bytes, udp_len);

        uint64_t now = now_ms();
        g_pkt_count++;
        g_byte_count += udp_len;

        if (now - g_last_ts >= 1000) {
            printf("[wirefisher] pps=%llu, bps=%llu\n",
                   (unsigned long long)g_pkt_count,
                   (unsigned long long)g_byte_count);
            g_pkt_count = 0;
            g_byte_count = 0;
            g_last_ts = now;
        }

        usleep(5000 + rand() % 45000);
    }

    close(g_hook_sock);
    unlink(HOOK_SOCK_PATH);
    return 0;
}

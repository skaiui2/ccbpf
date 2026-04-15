#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include "cal_udp.h"
#include "scp.h"
#include "scp_time.h"
#include "ccbpf.h"
#include "heap.h"
#include "hashmap.h"  

#define HOOK_SOCK_PATH "/tmp/ccbpf_hook_nodeA.sock"

#define SEND_BUF 2048
#define TEST_FILE_SIZE (10 * 1024 * 1024)



uint32_t native_ntohl(struct ccbpf_program *p,
                      uint32_t a0,
                      uint32_t a1,
                      uint32_t a2,
                      uint32_t a3)
{
    return ntohl(a0);
}

uint32_t native_ntohs(struct ccbpf_program *p,
                      uint32_t a0,
                      uint32_t a1,
                      uint32_t a2,
                      uint32_t a3)
{
    return ntohs((uint16_t)a0);
}

uint32_t native_printf(struct ccbpf_program *p,
                       uint32_t a0,
                       uint32_t a1,
                       uint32_t a2,
                       uint32_t a3)
{
    printf("%u", a0);
    return 0;
}

uint32_t native_print_str(struct ccbpf_program *p,
                          uint32_t a0,
                          uint32_t a1,
                          uint32_t a2,
                          uint32_t a3)
{
    if (a0 >= (uint32_t)p->string_count)
        return 0;

    printf("%s", p->strings[a0]);
    return 0;
}

uint32_t native_map_lookup(struct ccbpf_program *p,
                           uint32_t a0,
                           uint32_t a1,
                           uint32_t a2,
                           uint32_t a3)
{
    uint32_t map_id = a0;
    uint32_t key    = a1;

    if (map_id >= p->map_count)
        return 0;

    void *val_ptr = hashmap_get(&p->maps[map_id],
                                (void *)(uintptr_t)key);
    return val_ptr ? (uint32_t)(uintptr_t)val_ptr : 0;
}

uint32_t native_map_update(struct ccbpf_program *p,
                           uint32_t a0,
                           uint32_t a1,
                           uint32_t a2,
                           uint32_t a3)
{
    uint32_t map_id = a0;
    uint32_t key    = a1;
    uint32_t value  = a2;

    if (map_id >= p->map_count)
        return 0;

    hashmap_put(&p->maps[map_id],
                (void *)(uintptr_t)key,
                (void *)(uintptr_t)value);

    return 0;
}

uint32_t native_now_us(struct ccbpf_program *p,
                       uint32_t a0,
                       uint32_t a1,
                       uint32_t a2,
                       uint32_t a3)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t us = (uint64_t)ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000ULL;
    return (uint32_t)us;
}


uint32_t native_access(struct ccbpf_program *p,
                       uint32_t index,
                       uint32_t offset,
                       uint32_t size,
                       uint32_t a3)
{
    void *ptr = ((void **)p->ctx)[index];
    uint32_t val = 0;

    memcpy(&val, (char*)ptr + offset, size);
    return val;
}

uint32_t native_prints(struct ccbpf_program *p,
                       uint32_t index,
                       uint32_t a1,
                       uint32_t a2,
                       uint32_t a3)
{
    const char *s = ((const char **)p->ctx)[index];
    if (s)
        printf("%s", s);
    return 0;
}

void native_register_all(void)
{
    native_register(1, 1, native_ntohl);
    native_register(2, 1, native_ntohs);
    native_register(3, 2, native_printf);
    native_register(4, 1, native_print_str);
    native_register(5, 2, native_map_lookup);
    native_register(6, 3, native_map_update);
    native_register(7, 0, native_now_us);
    native_register(8, 3, native_access);
    native_register(9, 1, native_prints);
}



static int g_hook_sock = -1;

static int hook_control_init(void)
{
    int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0)
        return -1;

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
    if (!fp)
        return NULL;

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }

    long size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        return NULL;
    }
    rewind(fp);

    uint8_t *buf = heap_malloc((size_t)size);
    if (!buf) {
        fclose(fp);
        return NULL;
    }

    size_t n = fread(buf, 1, (size_t)size, fp);
    fclose(fp);

    if (n != (size_t)size) {
        heap_free(buf);
        return NULL;
    }

    *len = (size_t)size;
    return buf;
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

            size_t len = 0;
            uint8_t *image = load_file(path, &len);
            if (!image)
                continue;

            hook_attach(hook, image, len);
            heap_free(image);
        } else if (strncmp(buf, "DETACH ", 7) == 0) {
            char hook[64];
            if (sscanf(buf, "DETACH %63s", hook) != 1)
                continue;

            hook_detach(hook);
        }
    }
}


struct scp_udp_user {
    cal_udp_ctx_t *udp;
    struct sockaddr_in peer;
};

static int scp_udp_send(void *user, const void *buf, size_t len)
{
    struct scp_udp_user *u = user;
    return cal_udp_send(u->udp, buf, len, &u->peer);
}

int main(void)
{
    native_register_all();

    if (hook_control_init() < 0) {
        fprintf(stderr, "[A] hook_control_init failed\n");
        return 1;
    }

    printf("[A] generating %d bytes testA.bin...\n", TEST_FILE_SIZE);
    if (access("testA.bin", F_OK) != 0) {
        int gen = open("testA.bin", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (gen < 0) {
            perror("open testA.bin for write");
            return 1;
        }
        for (size_t i = 0; i < TEST_FILE_SIZE; i++) {
            uint8_t b = i % 256;
            if (write(gen, &b, 1) != 1) {
                perror("write testA.bin");
                close(gen);
                return 1;
            }
        }
        close(gen);
    } else {
        printf("[A] testA.bin exists, skip generating.\n");
    }

    int fd_send = open("testA.bin", O_RDONLY);
    if (fd_send < 0) {
        perror("open testA.bin for read");
        return 1;
    }

    int fd_recv = open("outA.bin", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_recv < 0) {
        perror("open outA.bin for write");
        close(fd_send);
        return 1;
    }

    cal_udp_ctx_t udp;
    if (cal_udp_open(&udp, "127.0.0.1", 5000) != 0) {
        fprintf(stderr, "[A] cal_udp_open failed\n");
        close(fd_send);
        close(fd_recv);
        return 1;
    }

    int fl = fcntl(udp.sockfd, F_GETFL, 0);
    fcntl(udp.sockfd, F_SETFL, fl | O_NONBLOCK);
    srand((unsigned)time(NULL));

    scp_init(16);
    scp_time_init();

    struct scp_udp_user user;
    user.udp = &udp;
    user.peer.sin_family = AF_INET;
    user.peer.sin_port   = htons(6000);
    user.peer.sin_addr.s_addr = inet_addr("127.0.0.1");

    struct scp_transport_class st = {
        .user  = &user,
        .send  = scp_udp_send,
        .recv  = NULL,
        .close = NULL
    };

    uint8_t sendbuf[SEND_BUF];
    uint8_t recvbuf[2048];
    uint8_t rxbuf[2048];
    struct sockaddr_in src;

    size_t sent = 0;
    size_t received = 0;

    struct scp_stream *ss = scp_stream_alloc(&st, 1, 1);

    scp_connect(1);

    printf("[A] waiting ESTABLISHED...\n");
    while (ss->state != SCP_ESTABLISHED) {
        hook_process_control_messages(); 
        scp_timer_process();

        int rn = cal_udp_recv(&udp, rxbuf, sizeof(rxbuf), &src);
        if (rn > 0)
            scp_input(ss, rxbuf, rn);

        usleep(1000);
    }

    printf("[A] ESTABLISHED, start full-duplex...\n");

    ssize_t cur_len = 0;
    size_t  cur_off = 0;
    int     have_pending = 0;

    while (1) {
        hook_process_control_messages();  

        scp_timer_process();

        int rn = cal_udp_recv(&udp, rxbuf, sizeof(rxbuf), &src);
        if (rn > 0)
            scp_input(ss, rxbuf, rn);

        if (!have_pending && sent < TEST_FILE_SIZE) {
            cur_len = read(fd_send, sendbuf, sizeof(sendbuf));
            if (cur_len > 0) {
                cur_off = 0;
                have_pending = 1;
            } else {
                have_pending = 0;
            }
        }

        if (have_pending) {
            int ret = scp_send(1, sendbuf + cur_off, (size_t)cur_len - cur_off);
            if (ret == 0) {
                sent += (size_t)cur_len;
                have_pending = 0;
            } else if (ret == -2) {
                /* EAGAIN, just retry later */
            } else {
                goto out;
            }
        }

        int n = scp_recv(1, recvbuf, sizeof(recvbuf));
        if (n > 0) {
            if (write(fd_recv, recvbuf, n) != n) {
                perror("write outA.bin");
                goto out;
            }
            received += (size_t)n;
        }

        if (sent == TEST_FILE_SIZE &&
            ss->snd_una == ss->snd_nxt &&
            received == TEST_FILE_SIZE) {

            printf("[A] full-duplex done, sending FIN...\n");
            scp_close(1);

            int wait_ms = 0;
            while (ss->state != SCP_CLOSED && wait_ms < 5000) {
                hook_process_control_messages();  
                scp_timer_process();
                int rn2 = cal_udp_recv(&udp, rxbuf, sizeof(rxbuf), &src);
                if (rn2 > 0)
                    scp_input(ss, rxbuf, rn2);
                usleep(1000);
                wait_ms++;
            }
            printf("[A] CLOSED or timeout, state=%d\n", ss->state);
            break;
        }

        usleep(1000);
    }

out:
    close(fd_send);
    close(fd_recv);

    if (g_hook_sock >= 0) {
        close(g_hook_sock);
        unlink(HOOK_SOCK_PATH);
    }

    printf("[A] ALL down!!!\n");
    return 0;
}

/*
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <string.h>
#include "cal_udp.h"
#include "scp.h"
#include "scp_time.h"

#define SEND_BUF 2048
#define TEST_FILE_SIZE (100 * 1024 * 1024)

struct scp_udp_user {
    cal_udp_ctx_t *udp;
    struct sockaddr_in peer;
};

static int scp_udp_send(void *user, const void *buf, size_t len)
{
    struct scp_udp_user *u = user;
    return cal_udp_send(u->udp, buf, len, &u->peer);
}

int main()
{
    printf("[A] generating %d bytes testA.bin...\n", TEST_FILE_SIZE);

    int gen = open("testA.bin", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    for (size_t i = 0; i < TEST_FILE_SIZE; i++) {
        uint8_t b = i % 256;
        write(gen, &b, 1);
    }
    close(gen);

    int fd_send = open("testA.bin", O_RDONLY);

    cal_udp_ctx_t udp;
    cal_udp_open(&udp, "127.0.0.1", 5000);

    int fl = fcntl(udp.sockfd, F_GETFL, 0);
    fcntl(udp.sockfd, F_SETFL, fl | O_NONBLOCK);
    srand(time(NULL));

    scp_init(16);
    scp_time_init();

    struct scp_udp_user user;
    user.udp = &udp;
    user.peer.sin_family = AF_INET;
    user.peer.sin_port   = htons(6000);
    user.peer.sin_addr.s_addr = inet_addr("127.0.0.1");

    struct scp_transport_class st = {
        .user  = &user,
        .send  = scp_udp_send,
        .recv  = NULL,
        .close = NULL
    };

    uint8_t sendbuf[SEND_BUF];
    uint8_t rxbuf[2048];
    struct sockaddr_in src;

    size_t sent = 0;

    struct scp_stream *ss = scp_stream_alloc(&st, 1, 1);

    scp_connect(1);

    printf("[A] waiting ESTABLISHED...\n");
    while (ss->state != SCP_ESTABLISHED) {
        scp_timer_process();
        int rn = cal_udp_recv(&udp, rxbuf, sizeof(rxbuf), &src);
        if (rn > 0) scp_input(ss, rxbuf, rn);
        usleep(1000);
    }

    printf("[A] ESTABLISHED, start one-way send...\n");

    ssize_t cur_len = 0;
    size_t  cur_off = 0;
    int     have_pending = 0;

    while (1) {
        scp_timer_process();

        int rn = cal_udp_recv(&udp, rxbuf, sizeof(rxbuf), &src);
        if (rn > 0) scp_input(ss, rxbuf, rn);

        if (!have_pending && sent < TEST_FILE_SIZE) {
            cur_len = read(fd_send, sendbuf, sizeof(sendbuf));
            if (cur_len > 0) {
                cur_off = 0;
                have_pending = 1;
            } else {
                have_pending = 0;
            }
        }

        if (have_pending) {
            int ret = scp_send(1, sendbuf + cur_off, (size_t)cur_len - cur_off);
            if (ret == 0) {
                sent += (size_t)cur_len;
                have_pending = 0;
            } else if (ret == -2) {
                // window full, wait
            } else {
                goto out;
            }
        }

        if (sent == TEST_FILE_SIZE &&
            ss->snd_una == ss->snd_nxt) {

            printf("[A] one-way send done, sending FIN...\n");
            scp_close(1);

            int wait_ms = 0;
            while (ss->state != SCP_CLOSED && wait_ms < 5000) {
                scp_timer_process();
                int rn2 = cal_udp_recv(&udp, rxbuf, sizeof(rxbuf), &src);
                if (rn2 > 0) scp_input(ss, rxbuf, rn2);
                usleep(1000);
                wait_ms++;
            }
            printf("[A] CLOSED or timeout, state=%d\n", ss->state);
            break;
        }

        usleep(1000);
    }

out:
    close(fd_send);
    printf("ALL down!!!\n");
    return 0;
}
*/

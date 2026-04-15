#include "cal_udp.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <fcntl.h>
#include <errno.h>

int cal_udp_open(cal_udp_ctx_t *ctx, const char *bind_ip, uint16_t bind_port)
{
    if (!ctx) return -1;

    ctx->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (ctx->sockfd < 0) {
        perror("udp socket");
        return -1;
    }

    memset(&ctx->local_addr, 0, sizeof(ctx->local_addr));
    ctx->local_addr.sin_family = AF_INET;
    ctx->local_addr.sin_port   = htons(bind_port);
    ctx->local_addr.sin_addr.s_addr =
        bind_ip ? inet_addr(bind_ip) : htonl(INADDR_ANY);

    if (bind(ctx->sockfd,
             (struct sockaddr*)&ctx->local_addr,
             sizeof(ctx->local_addr)) < 0)
    {
        perror("udp bind");
        close(ctx->sockfd);
        return -1;
    }

    return 0;
}

void cal_udp_close(cal_udp_ctx_t *ctx)
{
    if (!ctx) return;
    if (ctx->sockfd >= 0)
        close(ctx->sockfd);
    ctx->sockfd = -1;
}

int cal_udp_send(cal_udp_ctx_t *ctx,
                 const void *buf, size_t len,
                 const struct sockaddr_in *dst)
{
    if (!ctx || ctx->sockfd < 0 || !dst) return -1;

    ssize_t n = sendto(ctx->sockfd, buf, len, 0,
                       (const struct sockaddr*)dst,
                       sizeof(*dst));
    if (n < 0) {
        perror("udp sendto");
        return -1;
    }
    return (int)n;
}

int cal_udp_recv(cal_udp_ctx_t *ctx,
                 void *buf, size_t maxlen,
                 struct sockaddr_in *src)
{
    if (!ctx || ctx->sockfd < 0) return -1;

    socklen_t slen = sizeof(struct sockaddr_in);
    ssize_t n = recvfrom(ctx->sockfd, buf, maxlen, 0,
                         (struct sockaddr*)src, &slen);
    if (n < 0) { 
        if (errno == EAGAIN || errno == EWOULDBLOCK) { 
            return 0; 
        } 
        perror("udp recvfrom"); 
        return -1;
    }
    return (int)n;
}
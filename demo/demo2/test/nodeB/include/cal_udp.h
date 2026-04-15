#ifndef CAL_UDP_H
#define CAL_UDP_H

#include <stdint.h>
#include <stddef.h>
#include <netinet/in.h>

/*
 * UDP link context
 * 用于 send/recv 的最小封装
 */
typedef struct {
    int sockfd;
    struct sockaddr_in local_addr;
} cal_udp_ctx_t;

/*
 * 初始化 UDP（bind 到本地端口）
 * 返回 0 成功，-1 失败
 */
int cal_udp_open(cal_udp_ctx_t *ctx, const char *bind_ip, uint16_t bind_port);

/*
 * 关闭 UDP
 */
void cal_udp_close(cal_udp_ctx_t *ctx);

/*
 * 发送数据到指定地址
 * 返回发送字节数，或 -1 失败
 */
int cal_udp_send(cal_udp_ctx_t *ctx,
                 const void *buf, size_t len,
                 const struct sockaddr_in *dst);

/*
 * 接收数据（阻塞或非阻塞取决于 socket 设置）
 * 返回接收字节数，或 -1 失败
 */
int cal_udp_recv(cal_udp_ctx_t *ctx,
                 void *buf, size_t maxlen,
                 struct sockaddr_in *src);

#endif // CAL_UDP_H

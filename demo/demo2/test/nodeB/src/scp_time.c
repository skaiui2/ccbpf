#include <sys/timerfd.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include "scp_time.h"
#include "scp.h"

uint32_t scp_clock = 0;

static void *scp_time_thread(void *arg)
{
    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
    if (tfd < 0) {
        perror("timerfd_create");
        return NULL;
    }

    struct itimerspec its;
    memset(&its, 0, sizeof(its));

    its.it_interval.tv_sec  = 0;
    its.it_interval.tv_nsec = 1 * 1000 * 1000;   /* 1ms */

    its.it_value.tv_sec  = 0;
    its.it_value.tv_nsec = 1 * 1000 * 1000;

    if (timerfd_settime(tfd, 0, &its, NULL) < 0) {
        perror("timerfd_settime");
        close(tfd);
        return NULL;
    }

    uint64_t exp = 0;

    while (1) {
        ssize_t n = read(tfd, &exp, sizeof(exp));
        if (n != sizeof(exp)) {
            if (n < 0 && errno == EINTR)
                continue;
            perror("read(timerfd)");
            break;
        }

        scp_clock += (uint32_t)exp;
    }

    close(tfd);
    return NULL;
}

int scp_time_init(void)
{
    pthread_t tid;
    int ret = pthread_create(&tid, NULL, scp_time_thread, NULL);
    if (ret != 0) {
        errno = ret;
        perror("pthread_create(scp_time_thread)");
        return -1;
    }

    pthread_detach(tid);
    return 0;
}

uint32_t scp_now_time(void)
{
    return scp_clock;
}

#ifndef SCP_TIME_H
#define SCP_TIME_H
#include <stdint.h>
#include <time.h>

typedef void (*scp_timer_task)(void);
int scp_time_init(void); 

uint32_t scp_now_time(void);

#endif
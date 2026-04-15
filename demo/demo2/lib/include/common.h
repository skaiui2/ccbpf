#ifndef _COMMON_H
#define _COMMON_H

#include <stdint.h>

static inline unsigned short in_checksum(void *b, int len)
{
    unsigned short *addr = (unsigned short *)b;
    long sum = 0;

    for (; len > 1; len -= 2) {
        sum += *addr++;
        if (sum & 0x80000000) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }
    }
    if (len) {
        sum += *(unsigned char *)addr;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return ~sum;
}

static inline uint16_t swap16(uint16_t v)
{
    return (uint16_t)((v >> 8) | (v << 8));
}

static inline uint32_t swap32(uint32_t v)
{
    return ((v >> 24) & 0x000000FF) |
           ((v >>  8) & 0x0000FF00) |
           ((v <<  8) & 0x00FF0000) |
           ((v << 24) & 0xFF000000);
}


static inline uint16_t htons(uint16_t v) { return swap16(v); }
static inline uint16_t ntohs(uint16_t v) { return swap16(v); }

static inline uint32_t htonl(uint32_t v) { return swap32(v); }
static inline uint32_t ntohl(uint32_t v) { return swap32(v); }

#endif
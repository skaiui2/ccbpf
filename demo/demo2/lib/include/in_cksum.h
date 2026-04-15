#ifndef IN_CKSUM_H
#define IN_CKSUM_H

static inline unsigned short in_checksum(void *b, int len) 
{
    unsigned short *addr = (unsigned short *)b;
    long sum = 0;

    for (len; len > 1; len -= 2) {
        sum += *(unsigned short *)addr++;
        if (sum & 0x80000000) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }
    }
    if (len) {
        sum += (unsigned short)(*(unsigned char *)addr);
    }
    while(sum >> 16) {
        sum = (sum >> 16) + (sum & 0xFFFF);
    }

    return ~sum;
}

#endif
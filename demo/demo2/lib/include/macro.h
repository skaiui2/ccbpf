
#ifndef _MACRO_H
#define _MACRO_H

#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))


#define alignment_byte  0x07


#endif

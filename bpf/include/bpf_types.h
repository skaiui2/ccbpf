#ifndef BPF_TYPES_H
#define BPF_TYPES_H

#include <stdint.h>


enum {
    NATIVE_NTOHL = 1,
    NATIVE_NTOHS = 2,
    NATIVE_PRINTF = 3,
    NATIVE_MAP_LOOKUP = 4,
    NATIVE_MAP_UPDATE = 5,
};



/*
 * BSD-style integer aliases
 * These match the semantics of u_int, u_char, etc.
 * but are backed by fixed-width stdint types.
 */

typedef uint8_t     u_char;
typedef uint16_t    u_short;
typedef uint32_t    u_int;
typedef uint64_t    u_long;

typedef int8_t      int8;
typedef int16_t     int16;
typedef int32_t     int32;
typedef int64_t     int64;

typedef uint8_t     u8;
typedef uint16_t    u16;
typedef uint32_t    u32;
typedef uint64_t    u64;


#endif 

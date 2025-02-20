#ifndef __STDDEF_H
#define __STDDEF_H
#include "std/stddef.h"
#include "conf.h"

#define NULL ((void *)0)

#define ERR -1

// 成员 member 在 type 中的偏移量
#define offsetof(TYPE, MEMBER) ((uint64) & ((TYPE *)0)->MEMBER)

// 包含该成员 member 的结构体 type 的起始位置指针，经过ptr地址计算
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char uchar;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long uint64;

typedef uint64 pde_t;
typedef uint32 dev_t;
typedef uint32 mode_t;
typedef uint32 flags_t;

#define SET_FLAG(flags, flag) (*(flags) |= (flag))
#define CLEAR_FLAG(flags, flag) (*(flags) &= ~(flag))
#define TEST_FLAG(flags, flag) (*(flags) & (flag))

#define ENOSYS 38



#endif
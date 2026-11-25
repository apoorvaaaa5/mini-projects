#ifndef CORE_PORTME_H
#define CORE_PORTME_H

#include <stdint.h>

/* Enable software floating point for high-precision CoreMark timing & score */
#ifndef HAS_FLOAT
#define HAS_FLOAT 1
#endif

#ifndef HAS_TIME_H
#define HAS_TIME_H 0
#endif

#ifndef USE_CLOCK
#define USE_CLOCK 0
#endif

#ifndef HAS_STDIO
#define HAS_STDIO 0
#endif

#ifndef HAS_PRINTF
#define HAS_PRINTF 0
#endif

#ifndef COMPILER_VERSION
#ifdef __GNUC__
#define COMPILER_VERSION "GCC "__VERSION__
#else
#define COMPILER_VERSION "arm-none-eabi-gcc"
#endif
#endif

#ifndef COMPILER_FLAGS
#define COMPILER_FLAGS "-O1 -mcpu=cortex-m0plus"
#endif

#ifndef MEM_LOCATION
#define MEM_LOCATION "DYNAMIC_HEAP"
#endif

/* Data Types */
typedef int16_t   ee_s16;
typedef uint16_t  ee_u16;
typedef int32_t   ee_s32;
typedef double    ee_f32;
typedef uint8_t   ee_u8;
typedef uint32_t  ee_u32;
typedef ee_u32    ee_ptr_int;
typedef uint32_t  ee_size_t;

#ifndef NULL
#define NULL ((void *)0)
#endif

#define align_mem(x) (void *)(4 + (((ee_ptr_int)(x)-1) & ~3))

#define CORETIMETYPE ee_u32
typedef ee_u32 CORE_TICKS;

#ifndef SEED_METHOD
#define SEED_METHOD SEED_VOLATILE
#endif

#ifndef MEM_METHOD
#define MEM_METHOD MEM_MALLOC
#endif

#ifndef MULTITHREAD
#define MULTITHREAD 1
#define USE_PTHREAD 0
#define USE_FORK    0
#define USE_SOCKET  0
#endif

#ifndef MAIN_HAS_NOARGC
#define MAIN_HAS_NOARGC 1
#endif

#ifndef MAIN_HAS_NORETURN
#define MAIN_HAS_NORETURN 0
#endif

#ifndef ITERATIONS
#define ITERATIONS 2000
#endif

#define CLOCKS_PER_SEC 1000

/* Map core_main.c's main() to coremark_main() so it coexists cleanly with OS main() */
#define main coremark_main

extern ee_u32 default_num_contexts;

typedef struct CORE_PORTABLE_S
{
    ee_u8 portable_id;
} core_portable;

void portable_init(core_portable *p, int *argc, char *argv[]);
void portable_fini(core_portable *p);

#if !defined(PROFILE_RUN) && !defined(PERFORMANCE_RUN) && !defined(VALIDATION_RUN)
#define PERFORMANCE_RUN 1
#endif

int ee_printf(const char *fmt, ...);

#endif /* CORE_PORTME_H */

#include "coremark.h"
#include "core_portme.h"
#include <heap.h>
#include <console.h>
#include <scheduler.h>

#if VALIDATION_RUN
volatile ee_s32 seed1_volatile = 0x3415;
volatile ee_s32 seed2_volatile = 0x3415;
volatile ee_s32 seed3_volatile = 0x66;
#endif
#if PERFORMANCE_RUN
volatile ee_s32 seed1_volatile = 0x0;
volatile ee_s32 seed2_volatile = 0x0;
volatile ee_s32 seed3_volatile = 0x66;
#endif
#if PROFILE_RUN
volatile ee_s32 seed1_volatile = 0x8;
volatile ee_s32 seed2_volatile = 0x8;
volatile ee_s32 seed3_volatile = 0x8;
#endif
volatile ee_s32 seed4_volatile = ITERATIONS;
volatile ee_s32 seed5_volatile = 0;

/* Timing functions hooked to OS SysTick 1 ms periodic counter */
extern volatile uint32_t system_millis;

CORETIMETYPE barebones_clock(void)
{
    return system_millis;
}

#define GETMYTIME(_t)              (*_t = barebones_clock())
#define MYTIMEDIFF(fin, ini)       ((fin) - (ini))
#define TIMER_RES_DIVIDER          1
#define SAMPLE_TIME_IMPLEMENTATION 1
#define EE_TICKS_PER_SEC           (CLOCKS_PER_SEC / TIMER_RES_DIVIDER)

static CORETIMETYPE start_time_val, stop_time_val;

void start_time(void)
{
    GETMYTIME(&start_time_val);
}

void stop_time(void)
{
    GETMYTIME(&stop_time_val);
}

CORE_TICKS get_time(void)
{
    CORE_TICKS elapsed = (CORE_TICKS)(MYTIMEDIFF(stop_time_val, start_time_val));
    return elapsed;
}

secs_ret time_in_secs(CORE_TICKS ticks)
{
    secs_ret retval = ((secs_ret)ticks) / (secs_ret)EE_TICKS_PER_SEC;
    return retval;
}

ee_u32 default_num_contexts = 1;

void portable_init(core_portable *p, int *argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (sizeof(ee_ptr_int) != sizeof(ee_u8 *)) {
        ee_printf("ERROR! Please define ee_ptr_int to a type that holds a pointer!\n");
    }
    if (sizeof(ee_u32) != 4) {
        ee_printf("ERROR! Please define ee_u32 to a 32b unsigned type!\n");
    }
    p->portable_id = 1;
}

void portable_fini(core_portable *p)
{
    p->portable_id = 0;
}

/* Dynamic heap memory allocation hooks */
void *portable_malloc(ee_size_t size)
{
    return malloc((size_t)size);
}

void portable_free(void *p)
{
    free(p);
}

/* External CoreMark main entry */
extern int coremark_main(void);

/*
 * StrataHalo OS Task Entry Wrapper:
 * Spawns the CoreMark suite as a fully schedulable dynamic task.
 */
void task_coremark(void)
{
    console_puts("\r\n=======================================================\r\n");
    console_puts("   EEMBC CoreMark 1.0 Benchmark (StrataHalo OS)        \r\n");
    console_puts("   Target: RP2040 Cortex-M0+ @ 125 MHz                 \r\n");
    console_puts("   Workload: 2,000 Iterations (Floating Point Enabled) \r\n");
    console_puts("   Allocating 2000-byte data block from Dynamic Heap   \r\n");
    console_puts("=======================================================\r\n\r\n");

    coremark_main();

    console_puts("\r\n[OK] CoreMark Benchmark run completed successfully.\r\n");
}

#pragma once
#define _PLATFORM_H_

void plat_mem_init();
void plat_ascii_out(char);
void plat_stop_sim();
void plat_cpu_init();
unsigned long long plat_clint_timer_read();
void plat_clint_timer_compare(unsigned long long);

// Convenient ways to make your program slower

#ifndef __NVSTD_DELAY_H__
#define __NVSTD_DELAY_H__

#include <time.h>
#include "rtypes.h"

// Delay execution for a given number of milliseconds
static inline void delay_ms(u32 ms)
{
    clock_t tick_len, cur, prev;
    tick_len = (clock_t)ms * (CLOCKS_PER_SEC / 1000);
    cur = prev = clock();
    while ((cur - prev) < tick_len) { cur = clock(); }
}

#endif

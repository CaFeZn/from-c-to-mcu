#include <stdio.h>
#ifndef PROBE
#define PROBE 0
#endif
int main(void)
{
#if PROBE == 1
    volatile unsigned long mv = 3300ul;
    return printf("mv=%lu\n", mv) < 0;
#elif PROBE == 2
    volatile double v = 3.3;
    return printf("v=%.3f\n", v) < 0;
#else
    return 0;
#endif
}

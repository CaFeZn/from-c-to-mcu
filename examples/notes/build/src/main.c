#include <assert.h>
#include <stdio.h>
#include "clamp.h"
#ifndef REPORT_MODE
#define REPORT_MODE 1
#endif
int main(void)
{
    int low = clamp_value(-3);
    int high = clamp_value(42);
    assert(low == 0 && high == 10);
    assert(clamp_calls == 2);
    printf("clamp=%d,%d calls=%u mode=%d\n", low, high,
           clamp_calls, REPORT_MODE);
    return 0;
}

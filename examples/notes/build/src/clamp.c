#include "clamp.h"
#include "config.h"
unsigned clamp_calls = 0;
int clamp_value(int value)
{
    ++clamp_calls;
    if (value < 0) return 0;
    if (value > CLAMP_LIMIT) return CLAMP_LIMIT;
    return value;
}

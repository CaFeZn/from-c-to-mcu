#include "api.h"
__attribute__((weak)) unsigned on_event(unsigned value)
{
    return value + 1u;
}
unsigned dispatch(unsigned value)
{
    return on_event(value);
}

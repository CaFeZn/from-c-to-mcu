#include <stdint.h>

/* GCC/Clang board integration fragment; call breakpoint_round from main loop. */
/* Compile with -O0 and use seven distinct instruction addresses in Flash. */
volatile uint32_t breakpoint_visits;

#define POINT(n) \
    __attribute__((noinline, aligned(4))) void bp##n(void) \
    { breakpoint_visits += n; }

POINT(1)
POINT(2)
POINT(3)
POINT(4)
POINT(5)
POINT(6)
POINT(7)

void breakpoint_round(void)
{
    bp1(); bp2(); bp3(); bp4(); bp5(); bp6(); bp7();
}

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    uint32_t shared = 10u;
    uint32_t local = shared;  /* main: load */
    shared = shared + 1u;     /* ISR: complete increment */
    local = local + 1u;       /* main: calculate */
    shared = local;           /* main: store old result */
    assert(shared == 11u);
    printf("final=%lu, expected=12\n", (unsigned long)shared);
    return 0;
}

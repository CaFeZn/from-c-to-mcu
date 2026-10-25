#include <assert.h>
#include <stddef.h>
#include <stdio.h>

static int fd_length(unsigned dlc, unsigned *bytes)
{
    static const unsigned table[16] = {
        0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u,
        12u, 16u, 20u, 24u, 32u, 48u, 64u
    };
    if (dlc >= 16u || bytes == NULL) return 0;
    *bytes = table[dlc];
    return 1;
}

int main(void)
{
    unsigned bytes = 0u;
    assert(fd_length(11u, &bytes) && bytes == 20u);
    printf("DLC=11 bytes=%u\n", bytes);
    assert(fd_length(15u, &bytes) && bytes == 64u);
    printf("DLC=15 bytes=%u\n", bytes);
    assert(!fd_length(16u, &bytes));
    return 0;
}

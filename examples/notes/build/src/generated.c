#include <assert.h>
#include <stdio.h>
#include "generated.h"
int main(void)
{
    assert(RECORD_COUNT >= 1 && RECORD_COUNT <= 100);
    printf("records=%d\n", RECORD_COUNT);
    return 0;
}

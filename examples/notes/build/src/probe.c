#include <assert.h>
#include <stdio.h>
#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L
#error C99_or_later_required
#endif
int main(void)
{
    int sum = 0;
    for (int i = 1; i <= 10; ++i) sum += i;
    assert(sum == 55);
    puts("c99 ok: sum=55");
    return 0;
}

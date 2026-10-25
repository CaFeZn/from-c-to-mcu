#include <assert.h>
#include <limits.h>
#include <stdio.h>

static unsigned file_count;

static void visit(void)
{
    unsigned automatic = 10;
    static unsigned saved;
    ++automatic;
    ++saved;
    ++file_count;
    assert(automatic == 11 && saved == file_count);
    printf("automatic=%u saved=%u file=%u\n", automatic, saved, file_count);
}

int main(void)
{
    int original = 7;
    int copy = original;
    int *alias = &original;
    *alias = 9;
    assert(original == 9 && copy == 7 && alias == &original);
    printf("original=%d copy=%d alias_value=%d\n", original, copy, *alias);
    assert(file_count == 0);
    visit();
    visit();
    visit();
    printf("char_bits=%d int_bytes=%zu pointer_bytes=%zu\n",
           CHAR_BIT, sizeof original, sizeof alias);
    return 0;
}

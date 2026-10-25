#include <assert.h>
#include <stdio.h>

static int add(int a, int b)
{
    return a + b;
}

int main(void)
{
    int a = 4;
    int b = 7;
    int sum = add(a++, b++);
    assert(sum == 11 && a == 5 && b == 8);
    printf("independent: sum=%d a=%d b=%d\n", sum, a, b);

    int i = 3;
    int first = i++;
    int second = i++;
    assert(first == 3 && second == 4 && i == 5);
    printf("split: first=%d second=%d i=%d\n", first, second, i);

    int j = 0;
    int chain = (j++ == 0 && j++ == 1);
    int divisor = 0;
    int divided = (divisor != 0 && 12 / divisor > 1);
    assert(chain == 1 && j == 2 && divided == 0);
    printf("short: result=%d j=%d divided=%d\n", chain, j, divided);

    int k = 0;
    int chosen = (k++ == 0 ? k++ : 99);
    assert(chosen == 1 && k == 2);
    printf("conditional: chosen=%d k=%d\n", chosen, k);

    int marker = 0;
    int value = (marker++, marker + 10);
    assert(value == 11 && marker == 1);
    printf("comma: value=%d marker=%d\n", value, marker);
    return 0;
}

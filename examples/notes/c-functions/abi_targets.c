/* Kept in a separate translation unit; compile without LTO. */
unsigned abi_sum5(unsigned a, unsigned b, unsigned c, unsigned d, unsigned e)
{
    return a + b + c + d + e;
}

unsigned abi_twice(unsigned x)
{
    return x * 2u;
}

float abi_fadd(float x, float y)
{
    return x + y;
}

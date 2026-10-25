#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    const uint32_t base = UINT32_C(0x123);
    const uint32_t extension = UINT32_C(0x00005);
    uint32_t id;
    assert(base <= UINT32_C(0x7FF));
    assert(extension <= UINT32_C(0x3FFFF));
    id = (base << 18) | extension;
    assert(id == UINT32_C(0x048C0005));
    assert((id >> 18) == base);
    assert((id & UINT32_C(0x3FFFF)) == extension);
    printf("id=0x%08lX base=0x%03lX tail=0x%05lX\n",
           (unsigned long)id, (unsigned long)(id >> 18),
           (unsigned long)(id & UINT32_C(0x3FFFF)));
    return 0;
}

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if CHAR_BIT != 8
#error This wire-format example requires 8-bit bytes.
#endif

static uint32_t read_be32(const unsigned char b[4])
{
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16)
         | ((uint32_t)b[2] << 8) | (uint32_t)b[3];
}

int main(void)
{
    float original = 1.0f;
    unsigned char bytes[sizeof original];
    float restored = 0.0f;
    memcpy(bytes, &original, sizeof bytes);
    memcpy(&restored, bytes, sizeof restored);
    assert(restored == original);

    const unsigned char *view = (const unsigned char *)&original;
    for (size_t i = 0; i < sizeof original; ++i) {
        assert(view[i] == bytes[i]);
    }

    const unsigned char packet[5] = {0xff, 0x12, 0x34, 0x56, 0x78};
    uint32_t number = read_be32(packet + 1);
    assert(number == UINT32_C(0x12345678));
    printf("roundtrip=ok decoded=%08lx\n", (unsigned long)number);
    return 0;
}

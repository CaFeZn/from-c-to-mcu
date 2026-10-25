#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

struct Sample {
    uint8_t tag;
    uint32_t count;
    uint16_t code;
};

struct Compact {
    uint32_t count;
    uint16_t code;
    uint8_t tag;
};

#ifdef VERIFY_ARM_LAYOUT
typedef char sample_size[(sizeof(struct Sample) == 12) ? 1 : -1];
typedef char sample_count[(offsetof(struct Sample, count) == 4) ? 1 : -1];
typedef char sample_code[(offsetof(struct Sample, code) == 8) ? 1 : -1];
typedef char compact_size[(sizeof(struct Compact) == 8) ? 1 : -1];
#endif

int main(void)
{
    struct Sample pair[2] = {{1, 100, 2}, {3, 200, 4}};
    assert(offsetof(struct Sample, tag) == 0);
    assert(offsetof(struct Sample, count) >= sizeof(uint8_t));
    assert(offsetof(struct Sample, code) >=
           offsetof(struct Sample, count) + sizeof(uint32_t));
    assert(sizeof(struct Sample) >=
           offsetof(struct Sample, code) + sizeof(uint16_t));
    assert(sizeof pair == 2 * sizeof pair[0]);
    assert(pair[1].count == 200);
    printf("Sample: size=%zu tag=%zu count=%zu code=%zu\n",
           sizeof(struct Sample), offsetof(struct Sample, tag),
           offsetof(struct Sample, count), offsetof(struct Sample, code));
    printf("Compact: size=%zu count=%zu code=%zu tag=%zu\n",
           sizeof(struct Compact), offsetof(struct Compact, count),
           offsetof(struct Compact, code), offsetof(struct Compact, tag));
    printf("two_samples=%zu\n", sizeof pair);
    return 0;
}

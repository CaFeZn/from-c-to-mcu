#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define STR_RAW(x) #x
#define STR(x) STR_RAW(x)
#define CAT_RAW(a, b) a ## b
#define CAT(a, b) CAT_RAW(a, b)
#define CHANNEL 2

#define COMMAND_ROWS(X) \
    X(PING, 1)          \
    X(READ, 4)          \
    X(RESET, 9)

#define MAKE_ENUM(name, value) CMD_ ## name = (value),
enum command { COMMAND_ROWS(MAKE_ENUM) };
#undef MAKE_ENUM

typedef struct {
    int code;
    const char *name;
} command_info;

#define MAKE_INFO(name, value) { CMD_ ## name, #name },
static const command_info commands[] = { COMMAND_ROWS(MAKE_INFO) };
#undef MAKE_INFO

#define COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))
#define TRACE(...) do {           \
    fputs("[trace] ", stdout);    \
    printf(__VA_ARGS__);           \
    putchar('\n');                \
} while (0)

static const char *command_name(int code)
{
    for (size_t i = 0; i < COUNT_OF(commands); ++i) {
        if (commands[i].code == code) {
            return commands[i].name;
        }
    }
    return "UNKNOWN";
}

static inline int max_int(int a, int b)
{
    return a > b ? a : b;
}

int main(void)
{
    int CAT(channel_, CHANNEL) = 7;
    assert(strcmp(STR_RAW(CHANNEL), "CHANNEL") == 0);
    assert(strcmp(STR(CHANNEL), "2") == 0);
    assert(channel_2 == 7);
    printf("raw=%s expanded=%s channel=%d\n",
           STR_RAW(CHANNEL), STR(CHANNEL), channel_2);

    assert(COUNT_OF(commands) == 3);
    assert(CMD_PING == 1 && CMD_READ == 4 && CMD_RESET == 9);
    assert(strcmp(command_name(1), "PING") == 0);
    assert(strcmp(command_name(4), "READ") == 0);
    assert(strcmp(command_name(9), "RESET") == 0);
    assert(strcmp(command_name(-1), "UNKNOWN") == 0);
    assert(strcmp(command_name(0), "UNKNOWN") == 0);
    assert(strcmp(command_name(10), "UNKNOWN") == 0);
    printf("names=%s,%s,%s missing=%s\n", command_name(1),
           command_name(4), command_name(9), command_name(0));

    int n = 3;
    int best = max_int(n++, 0);
    assert(best == 3 && n == 4);
    printf("max=%d next=%d\n", best, n);

    if (channel_2 == 7)
        TRACE("ready");
    else
        TRACE("unexpected");
    TRACE("count=%u", (unsigned)COUNT_OF(commands));
    return 0;
}

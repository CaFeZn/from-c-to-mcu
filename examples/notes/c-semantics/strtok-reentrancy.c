#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* Each parsing session owns its cursor and writable input. */
static char *next_token(char **cursor, const char *delimiters)
{
    char *start;
    char *end;

    if (cursor == NULL || *cursor == NULL || delimiters == NULL) {
        return NULL;
    }
    start = *cursor;
    start += strspn(start, delimiters);
    if (*start == '\0') {
        *cursor = NULL;
        return NULL;
    }
    end = start + strcspn(start, delimiters);
    if (*end == '\0') {
        *cursor = NULL;
    } else {
        *end = '\0';
        *cursor = end + 1;
    }
    return start;
}

static void expect_token(const char *actual, const char *expected)
{
    assert(actual != NULL);
    assert(strcmp(actual, expected) == 0);
}

int main(void)
{
    char broken[] = "led,on;motor,off";
    char fixed[] = "led,on;motor,off";
    const char *names[] = {"led", "motor"};
    const char *values[] = {"on", "off"};
    char *row;
    char *token;
    char *rows = fixed;
    size_t count = 0;
    char empty[] = "";
    char only_separators[] = ",,;";
    char repeated[] = ",a,,b,";
    char unchanged[] = "a,b";
    char mixed[] = "a,b;c";
    char changed[] = "a:b,c";
    char left[] = "L1 L2";
    char right[] = "R1 R2";
    char *cursor;
    char *left_cursor = left;
    char *right_cursor = right;

    row = strtok(broken, ";");
    expect_token(row, "led,on");
    token = strtok(row, ",");
    expect_token(token, "led");
    token = strtok(NULL, ",");
    expect_token(token, "on");
    row = strtok(NULL, ";");
    assert(row == NULL);
    puts("strtok: rows=1 (outer state lost)");

    while ((row = next_token(&rows, ";")) != NULL) {
        char *fields = row;
        assert(count < 2);
        token = next_token(&fields, ",");
        expect_token(token, names[count]);
        token = next_token(&fields, ",");
        expect_token(token, values[count]);
        token = next_token(&fields, ",");
        assert(token == NULL);
        ++count;
    }
    assert(count == 2 && rows == NULL);
    puts("cursor: rows=2 (independent state)");

    cursor = empty;
    token = next_token(&cursor, ",");
    assert(token == NULL && cursor == NULL);
    cursor = only_separators;
    token = next_token(&cursor, ",;");
    assert(token == NULL && cursor == NULL);
    cursor = repeated;
    token = next_token(&cursor, ",");
    expect_token(token, "a");
    token = next_token(&cursor, ",");
    expect_token(token, "b");
    token = next_token(&cursor, ",");
    assert(token == NULL && cursor == NULL);
    token = next_token(&cursor, ",");
    assert(token == NULL);
    cursor = unchanged;
    token = next_token(&cursor, "");
    expect_token(token, "a,b");
    token = next_token(&cursor, "");
    assert(token == NULL);
    cursor = mixed;
    token = next_token(&cursor, ",;");
    expect_token(token, "a");
    token = next_token(&cursor, ",;");
    expect_token(token, "b");
    token = next_token(&cursor, ",;");
    expect_token(token, "c");
    token = next_token(&cursor, ",;");
    assert(token == NULL);
    cursor = changed;
    token = next_token(&cursor, ":");
    expect_token(token, "a");
    token = next_token(&cursor, ",");
    expect_token(token, "b");
    token = next_token(&cursor, ",");
    expect_token(token, "c");
    token = next_token(&left_cursor, " ");
    expect_token(token, "L1");
    token = next_token(&right_cursor, " ");
    expect_token(token, "R1");
    token = next_token(&left_cursor, " ");
    expect_token(token, "L2");
    token = next_token(&right_cursor, " ");
    expect_token(token, "R2");
    token = next_token(NULL, ",");
    assert(token == NULL);
    cursor = unchanged;
    token = next_token(&cursor, NULL);
    assert(token == NULL && cursor == unchanged);
    puts("edges: empty repeated delimiters interleaving ok");
    puts("strtok-reentrancy: ok");
    return 0;
}

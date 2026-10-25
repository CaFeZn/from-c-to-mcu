/* Do not call this with INT_MAX. It is compiled, never executed here. */
int increased(int x) {
    return x + 1 > x;
}

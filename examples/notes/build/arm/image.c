volatile unsigned initialized = 7;
volatile unsigned zeros[4];
volatile unsigned observed;
int main(void)
{
    observed = initialized + zeros[0];
    for (;;) { }
}

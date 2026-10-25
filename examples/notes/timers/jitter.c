#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>

int main(void)
{
    const double period_ns[] = {998.0, 1002.0, 999.0, 1001.0};
    const size_t n = sizeof period_ns / sizeof period_ns[0];
    double sum = 0.0;
    for (size_t i = 0; i < n; ++i) {
        sum += period_ns[i];
    }
    const double mean = sum / (double)n;
    double squares = 0.0;
    for (size_t i = 0; i < n; ++i) {
        double error = period_ns[i] - mean;
        squares += error * error;
    }
    const double rms = sqrt(squares / (double)n);
    assert(mean == 1000.0);
    assert(fabs(rms - 1.58113883008419) < 1e-10);
    printf("mean=%.3f ns, rms=%.3f ns\n", mean, rms);
    return 0;
}

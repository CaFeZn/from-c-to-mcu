#include "api.h"
volatile unsigned callback_result;
int main(void)
{
    callback_result = dispatch(7u);
    for (;;) { }
}

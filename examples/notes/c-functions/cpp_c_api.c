#include "cpp_c_api.h"

int packet_size_valid(unsigned length)
{
    return length > 0u && length <= 4u;
}

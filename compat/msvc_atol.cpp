#include "msvc_atol.h"

int32_t iwd2r_atol(const char* text)
{
    const unsigned char* p = reinterpret_cast<const unsigned char*>(text);
    while (*p == ' ' || (*p >= '\t' && *p <= '\r')) {
        ++p;
    }
    const unsigned char sign = *p;
    if (sign == '-' || sign == '+') {
        ++p;
    }
    uint32_t total = 0;
    while (*p >= '0' && *p <= '9') {
        total = total * 10u + static_cast<uint32_t>(*p - '0');
        ++p;
    }
    if (sign == '-') {
        total = 0u - total;
    }
    return static_cast<int32_t>(total);
}

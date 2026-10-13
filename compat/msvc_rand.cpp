#include "msvc_rand.h"

namespace {
// A thread that never seeds starts as if seeded with 1, as in VC6.
thread_local unsigned int g_state = 1;
} // namespace

int iwd2r_rand()
{
    g_state = g_state * 214013u + 2531011u;
    return static_cast<int>((g_state >> 16) & 0x7FFFu);
}

void iwd2r_srand(unsigned int seed)
{
    g_state = seed;
}

unsigned int iwd2r_rand_state()
{
    return g_state;
}

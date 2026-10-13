// iwd2r. Distributed under LICENSE.md.
// The VC6 C runtime's rand(): a per-thread linear congruential generator, so
// engine code rolls the same dice on every platform as the original exe.
#pragma once

int iwd2r_rand();
void iwd2r_srand(unsigned int seed);
// The current thread's generator state, for dumps.
unsigned int iwd2r_rand_state();

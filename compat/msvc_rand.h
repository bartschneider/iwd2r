// The VC6 C runtime's rand(): a per-thread linear congruential generator, so
// engine code rolls the same dice on every platform as the original exe.
#pragma once

int iwd2r_rand();
void iwd2r_srand(unsigned int seed);
// The current thread's generator state, for dumps.
unsigned int iwd2r_rand_state();
// The current thread's generator state itself, for reading it from another
// thread (the replay dump reads the input thread's while that thread waits).
// Valid until the thread exits.
const unsigned int* iwd2r_rand_state_address();
// The frame number IWD2R_RAND_TRACE writes with each call (docs/rand-trace.md);
// the replay mode sets it every frame.
void iwd2r_rand_trace_frame(unsigned int frame);
unsigned int iwd2r_rand_trace_frame_value();

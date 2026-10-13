// Our engine's side of the self-test. Engine headers bring compat's
// win_types.h, which clashes with <windows.h>, so selftest.cpp reaches our
// functions through this bridge.
#pragma once

namespace selftest_ours {

// iwd2r_srand(seed), then our IcewindMisc::Roll(nRolls, nSides).
int SeededRoll(unsigned int seed, int nRolls, int nSides);
// Our iwd2r_rand(): the next value of the calling thread's generator.
int NextRand();

} // namespace selftest_ours

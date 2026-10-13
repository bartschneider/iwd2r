// Calls into our engine objects linked into the runner (selftest_ours.h).
#include "oracle/cases/selftest_ours.h"

#include "IcewindMisc.h"
#include "msvc_rand.h"

namespace selftest_ours {

int SeededRoll(unsigned int seed, int nRolls, int nSides)
{
    iwd2r_srand(seed);
    return IcewindMisc::Roll(nRolls, nSides);
}

int NextRand()
{
    return iwd2r_rand();
}

} // namespace selftest_ours

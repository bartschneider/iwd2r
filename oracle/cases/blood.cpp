// CBlood's particles, the original's against ours.
//
// This file includes engine headers and so must not include <windows.h>: it
// reaches the original only through CallOriginal and fixed addresses.
//
// Fixtures. The original's CBlood is a zeroed block of its size from its
// operator new, built by its own constructor (0x5195B0), which constructs the
// two particle lists (CPtrList::CPtrList(10), 0x7FB239), sets the members and
// runs CalcBoundingRect (0x519C40) and GenerateParticles (0x51A500); ours is
// a CBlood from our constructor. Every blood type but the exploding death is
// built (its bounding rectangle reads the active engine's video mode); the
// exploding death's particles are compared through GenerateExplodingBlood
// (0x51A2A0) called on a pulsating artery on both sides. The constructors
// and the updates draw rand(), so each comparison starts from the same srand
// seed on both sides and compares the next rand value after it. Original
// objects are never freed.
//
// The original's layout (the constructor at 0x5195CA-0x51965C): the two
// CPtrLists at 0x04 and 0x20 (their head node at +0x04, the count at +0x0C;
// a node is next, previous, data), the reference point at 0x3C, the bounding
// rectangle at 0x5E, the time stamp at 0x50 and the particle type at 0x70. A
// CParticle (its copy constructor, 0x7C35B0): the time stamp, tail length
// and render time as shorts at 0x00, 0x02 and 0x04, the colour at 0x06, the
// type at 0x0A, the life span at 0x0C, the tag at 0x10, the position at 0x12
// and the velocity at 0x1E (three LONGs each), the gravity at 0x2A.
//
// Calling conventions (objdump of the vanilla exe): __thiscall, called here
// through __fastcall pointers whose edx argument is unused.
#include "CBlood.h"
#include "CParticle.h"
#include "msvc_rand.h"

#include "oracle/runner/compare.h"
#include "oracle/runner/generators.h"
#include "oracle/runner/registry.h"
#include "vanilla_config.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

using Srand = void(__cdecl*)(unsigned int);
using Rand = int(__cdecl*)();
// operator new (0x7FC95B): __cdecl, the size at [esp+4], plain ret.
using OriginalNewFn = void*(__cdecl*)(size_t);
constexpr uintptr_t kOriginalNew = 0x7FC95B;

// CBlood::CBlood(pArea, nX, nY, nZ, nDirection, nType, nCharacterHeight)
// (0x5195B0): seven stack arguments (the shorts pushed as dwords), `ret
// 0x1c` at 0x51967D.
using BloodCtorFn = void*(__fastcall*)(void* self, void* edx, void* pArea, int32_t nX, int32_t nY, int32_t nZ,
    int32_t nDirection, uint32_t nType, int32_t nCharacterHeight);
constexpr uintptr_t kBloodCtor = 0x5195B0;
// CBlood::AsynchronousUpdate() (0x519740): BOOLEAN in al, plain ret.
using BloodUpdateFn = uint8_t(__fastcall*)(void* self, void* edx);
constexpr uintptr_t kBloodUpdate = 0x519740;
// CBlood::GenerateExplodingBlood(nX, nY, nZ) (0x51A2A0): SHORT in ax,
// `ret 0xc`.
using ExplodingFn = int16_t(__fastcall*)(void* self, void* edx, int32_t nX, int32_t nY, int32_t nZ);
constexpr uintptr_t kExploding = 0x51A2A0;

// Up to m_bLeavePool (0x78), rounded up.
constexpr size_t kBloodSize = 0x7C;

constexpr size_t kParticleList = 0x04;
constexpr size_t kSplashList = 0x20;

void OriginalSrand(unsigned int seed)
{
    reinterpret_cast<Srand>(static_cast<uintptr_t>(kVanillaSrand))(seed);
}

int OriginalRand()
{
    return reinterpret_cast<Rand>(static_cast<uintptr_t>(kVanillaRand))();
}

uint8_t* OriginalBlock(size_t size)
{
    auto* at = static_cast<uint8_t*>(reinterpret_cast<OriginalNewFn>(kOriginalNew)(size));
    std::memset(at, 0, size);
    return at;
}

template <class T>
T At(const uint8_t* base, size_t offset)
{
    T value;
    std::memcpy(&value, base + offset, sizeof(T));
    return value;
}

std::string ParticleText(int16_t timeStamp, int16_t tail, int16_t renderTime, uint32_t color, uint16_t type,
    int32_t lifeSpan, uint8_t tag, const int32_t (&pos)[3], const int32_t (&vel)[3], int32_t gravity)
{
    return "time " + std::to_string(timeStamp) + ", tail " + std::to_string(tail) + ", render "
        + std::to_string(renderTime) + ", colour " + std::to_string(color) + ", type " + std::to_string(type)
        + ", life " + std::to_string(lifeSpan) + ", tag " + std::to_string(tag) + ", pos " + std::to_string(pos[0])
        + " " + std::to_string(pos[1]) + " " + std::to_string(pos[2]) + ", vel " + std::to_string(vel[0]) + " "
        + std::to_string(vel[1]) + " " + std::to_string(vel[2]) + ", gravity " + std::to_string(gravity);
}

// The particles of one of the original's lists, as text, in order.
std::vector<std::string> OriginalParticles(const uint8_t* blood, size_t list)
{
    std::vector<std::string> particles;
    const uint8_t* node = At<const uint8_t*>(blood, list + 0x04);
    while (node != nullptr) {
        const uint8_t* p = At<const uint8_t*>(node, 0x08);
        const int32_t pos[3] = { At<int32_t>(p, 0x12), At<int32_t>(p, 0x16), At<int32_t>(p, 0x1A) };
        const int32_t vel[3] = { At<int32_t>(p, 0x1E), At<int32_t>(p, 0x22), At<int32_t>(p, 0x26) };
        particles.push_back(ParticleText(At<int16_t>(p, 0x00), At<int16_t>(p, 0x02), At<int16_t>(p, 0x04),
            At<uint32_t>(p, 0x06), At<uint16_t>(p, 0x0A), At<int32_t>(p, 0x0C), At<uint8_t>(p, 0x10), pos, vel,
            At<int32_t>(p, 0x2A)));
        node = At<const uint8_t*>(node, 0x00);
    }
    return particles;
}

std::vector<std::string> OurParticles(CTypedPtrList<CPtrList, CParticle*>& list)
{
    std::vector<std::string> particles;
    POSITION pos = list.GetHeadPosition();
    while (pos != NULL) {
        const CParticle* p = list.GetNext(pos);
        const int32_t position[3] = { p->m_pos.x, p->m_pos.y, p->m_pos.z };
        const int32_t velocity[3] = { p->m_vel.x, p->m_vel.y, p->m_vel.z };
        particles.push_back(ParticleText(p->m_nTimeStamp, p->m_nTailLength, p->m_nRenderTime, p->m_rgbColor,
            p->m_wType, p->m_nLifeSpan, p->m_bTag, position, velocity, p->m_nGravity));
    }
    return particles;
}

// Both lists of both bloods, particle by particle; a list's comparison stops
// at its first difference.
void CompareLists(CaseContext& ctx, const uint8_t* original, CBlood& ours)
{
    const std::string inputs = ctx.Inputs();
    const size_t lists[2] = { kParticleList, kSplashList };
    const char* names[2] = { "particles", "splashes" };
    for (int n = 0; n < 2; ++n) {
        const std::vector<std::string> o = OriginalParticles(original, lists[n]);
        const std::vector<std::string> u = OurParticles(n == 0 ? ours.m_particleList : ours.m_lstSplashParticles);
        ctx.SetInputs(inputs + ", " + names[n] + " count");
        if (!oracle::Compare(ctx, o.size(), u.size())) {
            continue;
        }
        for (size_t i = 0; i < o.size(); ++i) {
            ctx.SetInputs(inputs + ", " + names[n] + " " + std::to_string(i));
            if (!oracle::Compare(ctx, o[i], u[i])) {
                break;
            }
        }
    }
    ctx.SetInputs(inputs);
}

struct Input {
    int32_t x;
    int32_t y;
    int32_t z;
    int16_t direction;
    uint16_t type;
    int32_t height;
    unsigned int seed;
};

std::string InputText(const Input& in)
{
    return "srand(" + std::to_string(in.seed) + "), CBlood(" + std::to_string(in.x) + ", " + std::to_string(in.y)
        + ", " + std::to_string(in.z) + ", direction " + std::to_string(in.direction) + ", type "
        + std::to_string(in.type) + ", height " + std::to_string(in.height) + ")";
}

uint8_t* OriginalBlood(const Input& in)
{
    uint8_t* blood = OriginalBlock(kBloodSize);
    reinterpret_cast<BloodCtorFn>(kBloodCtor)(blood, nullptr, nullptr, in.x, in.y, in.z, in.direction, in.type,
        in.height);
    return blood;
}

// Types 0-6 at every direction 0-15, then --iterations random bloods.
std::vector<Input> Inputs(const CaseContext& ctx, uint64_t salt)
{
    std::vector<Input> inputs;
    auto gen = oracle::Random<uint32_t>(ctx.seed ^ salt);
    for (uint16_t type = 0; type <= 6; ++type) {
        for (int16_t direction = 0; direction < 16; ++direction) {
            inputs.push_back({ 400, 300, 20, direction, type, 50, *gen.Next() });
        }
    }
    for (int i = 0; i < ctx.iterations / 100; ++i) {
        Input in;
        in.x = static_cast<int32_t>(*gen.Next() % 3000);
        in.y = static_cast<int32_t>(*gen.Next() % 3000);
        in.z = static_cast<int32_t>(*gen.Next() % 120);
        in.direction = static_cast<int16_t>(*gen.Next() % 16);
        in.type = static_cast<uint16_t>(*gen.Next() % 7);
        in.height = static_cast<int32_t>(*gen.Next() % 150);
        in.seed = *gen.Next();
        inputs.push_back(in);
    }
    return inputs;
}

// CBlood::CBlood (0x5195B0) with CalcBoundingRect (0x519C40) and
// GenerateParticles (0x51A500): the reference point, the bounding rectangle,
// the particle type, every particle and the next rand value.
void Particles(CaseContext& ctx)
{
    for (const Input& in : Inputs(ctx, 0x426C6F6F64500000ull)) { // "BloodP"
        ctx.SetInputs(InputText(in));
        uint8_t* original = nullptr;
        int originalNext = 0;
        ctx.CallOriginal([&] {
            OriginalSrand(in.seed);
            original = OriginalBlood(in);
            originalNext = OriginalRand();
        });

        iwd2r_srand(in.seed);
        CBlood ours(NULL, in.x, in.y, in.z, in.direction, in.type, in.height);
        const int oursNext = iwd2r_rand();

        ctx.CountInputs(1);
        const std::string inputs = ctx.Inputs();
        ctx.SetInputs(inputs + ", reference point");
        oracle::Compare(ctx, At<int32_t>(original, 0x3C), static_cast<int32_t>(ours.m_refPoint.x));
        oracle::Compare(ctx, At<int32_t>(original, 0x40), static_cast<int32_t>(ours.m_refPoint.y));
        ctx.SetInputs(inputs + ", bounding rectangle");
        oracle::Compare(ctx, At<int32_t>(original, 0x5E), static_cast<int32_t>(ours.m_rBounding.left));
        oracle::Compare(ctx, At<int32_t>(original, 0x62), static_cast<int32_t>(ours.m_rBounding.top));
        oracle::Compare(ctx, At<int32_t>(original, 0x66), static_cast<int32_t>(ours.m_rBounding.right));
        oracle::Compare(ctx, At<int32_t>(original, 0x6A), static_cast<int32_t>(ours.m_rBounding.bottom));
        ctx.SetInputs(inputs + ", particle type");
        oracle::Compare(ctx, At<uint16_t>(original, 0x70), ours.m_particleType);
        ctx.SetInputs(inputs);
        CompareLists(ctx, original, ours);
        ctx.SetInputs(inputs + ", the next rand");
        oracle::Compare(ctx, originalNext, oursNext);
    }
}

// CBlood::GenerateExplodingBlood (0x51A2A0) on a pulsating artery: the count
// it returns, every particle and the next rand value.
void Exploding(CaseContext& ctx)
{
    auto gen = oracle::Random<uint32_t>(ctx.seed ^ 0x426C6F6F64450000ull); // "BloodE"
    for (int i = 0; i < 64 + ctx.iterations / 100; ++i) {
        Input in = { static_cast<int32_t>(*gen.Next() % 3000), static_cast<int32_t>(*gen.Next() % 3000),
            static_cast<int32_t>(*gen.Next() % 120), static_cast<int16_t>(*gen.Next() % 16), 6,
            static_cast<int32_t>(*gen.Next() % 150), *gen.Next() };
        const int32_t nX = static_cast<int32_t>(*gen.Next() % 200000);
        const int32_t nY = static_cast<int32_t>(*gen.Next() % 200000);
        const int32_t nZ = static_cast<int32_t>(*gen.Next() % 20000);
        ctx.SetInputs(InputText(in) + ", GenerateExplodingBlood(" + std::to_string(nX) + ", " + std::to_string(nY)
            + ", " + std::to_string(nZ) + ")");

        uint8_t* original = nullptr;
        int16_t originalCount = 0;
        int originalNext = 0;
        ctx.CallOriginal([&] {
            OriginalSrand(in.seed);
            original = OriginalBlood(in);
            originalCount = reinterpret_cast<ExplodingFn>(kExploding)(original, nullptr, nX, nY, nZ);
            originalNext = OriginalRand();
        });

        iwd2r_srand(in.seed);
        CBlood ours(NULL, in.x, in.y, in.z, in.direction, in.type, in.height);
        const int16_t oursCount = ours.GenerateExplodingBlood(nX, nY, nZ);
        const int oursNext = iwd2r_rand();

        ctx.CountInputs(1);
        const std::string inputs = ctx.Inputs();
        ctx.SetInputs(inputs + ", count");
        oracle::Compare(ctx, originalCount, oursCount);
        ctx.SetInputs(inputs + ", particle type");
        oracle::Compare(ctx, At<uint16_t>(original, 0x70), ours.m_particleType);
        ctx.SetInputs(inputs);
        CompareLists(ctx, original, ours);
        ctx.SetInputs(inputs + ", the next rand");
        oracle::Compare(ctx, originalNext, oursNext);
    }
}

// CBlood::AsynchronousUpdate (0x519740) until the blood is done (at most 400
// calls): what each call returns, the time stamp and both lists after each,
// and the next rand value at the end.
void Update(CaseContext& ctx)
{
    for (const Input& in : Inputs(ctx, 0x426C6F6F64550000ull)) { // "BloodU"
        const std::string base = InputText(in);
        ctx.SetInputs(base);
        uint8_t* original = nullptr;
        ctx.CallOriginal([&] {
            OriginalSrand(in.seed);
            original = OriginalBlood(in);
        });
        iwd2r_srand(in.seed);
        CBlood ours(NULL, in.x, in.y, in.z, in.direction, in.type, in.height);

        for (int call = 1; call <= 400; ++call) {
            ctx.SetInputs(base + ", update " + std::to_string(call));
            uint8_t originalLeft = 0;
            ctx.CallOriginal([&] { originalLeft = reinterpret_cast<BloodUpdateFn>(kBloodUpdate)(original, nullptr); });
            const uint8_t oursLeft = ours.AsynchronousUpdate();

            const std::string inputs = ctx.Inputs();
            ctx.SetInputs(inputs + ", result");
            const bool same = oracle::Compare(ctx, originalLeft != 0, oursLeft != 0);
            ctx.SetInputs(inputs + ", time stamp");
            oracle::Compare(ctx, At<int16_t>(original, 0x50), ours.m_nTimeStamp);
            ctx.SetInputs(inputs);
            CompareLists(ctx, original, ours);
            if (!same || originalLeft == 0) {
                break;
            }
        }

        int originalNext = 0;
        ctx.CallOriginal([&] { originalNext = OriginalRand(); });
        const int oursNext = iwd2r_rand();
        ctx.CountInputs(1);
        ctx.SetInputs(base + ", the next rand after the updates");
        oracle::Compare(ctx, originalNext, oursNext);
    }
}

} // namespace

ORACLE_CASE("blood/particles", Particles);
ORACLE_CASE("blood/exploding", Exploding);
ORACLE_CASE("blood/update", Update);

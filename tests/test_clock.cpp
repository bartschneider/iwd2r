// iwd2r. Distributed under LICENSE.md.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "platform/clock.h"
#include "platform/win32_api.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace {

struct FakeClock : platform::ClockSource {
    std::atomic<uint64_t> value { 0 };
    uint64_t NowMs() override { return value.load(); }
};

std::atomic<int> g_calls { 0 };
std::atomic<DWORD_PTR> g_user { 0 };
std::atomic<UINT> g_id { 0 };

void CALLBACK CountingCallback(UINT id, UINT, DWORD_PTR user, DWORD_PTR, DWORD_PTR)
{
    g_id = id;
    g_user = user;
    ++g_calls;
}

void SleepMs(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

template <typename Pred>
bool WaitFor(Pred pred, int timeoutMs)
{
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (pred())
            return true;
        SleepMs(2);
    }
    return pred();
}

} // namespace

TEST_CASE("clock default GetTickCount advances", "[clock]")
{
    DWORD a = GetTickCount();
    Sleep(10);
    DWORD b = GetTickCount();
    REQUIRE(b >= a);
    REQUIRE(b - a >= 10);
    DWORD c = timeGetTime();
    REQUIRE(c >= b);
    REQUIRE(SleepEx(1, TRUE) == 0);
    Sleep(0);
}

TEST_CASE("clock source can be swapped and restored", "[clock]")
{
    FakeClock fake;
    platform::SetClockSource(&fake);
    fake.value = 1234;
    REQUIRE(GetTickCount() == 1234);
    REQUIRE(timeGetTime() == 1234);
    fake.value = 0x100000005ull;
    REQUIRE(GetTickCount() == 5);
    platform::SetClockSource(nullptr);
    REQUIRE(platform::DefaultClockSource() != nullptr);
    Sleep(5);
    REQUIRE(GetTickCount() != 5);
    REQUIRE(GetTickCount() < 0x10000000u);
}

TEST_CASE("clock periodic timer fires and stops on kill", "[clock]")
{
    g_calls = 0;
    g_user = 0;
    MMRESULT id = timeSetEvent(20, 1, CountingCallback, 42, TIME_PERIODIC);
    REQUIRE(id != 0);
    REQUIRE(WaitFor([] { return g_calls.load() >= 3; }, 500));
    REQUIRE(g_user == 42);
    REQUIRE(g_id == id);
    REQUIRE(timeKillEvent(id) == TIMERR_NOERROR);
    int after = g_calls.load();
    SleepMs(150);
    REQUIRE(g_calls.load() == after);
    REQUIRE(timeKillEvent(id) == MMSYSERR_INVALPARAM);
}

TEST_CASE("clock one-shot timer fires exactly once", "[clock]")
{
    g_calls = 0;
    MMRESULT id = timeSetEvent(10, 1, CountingCallback, 7, TIME_ONESHOT);
    REQUIRE(id != 0);
    REQUIRE(WaitFor([] { return g_calls.load() >= 1; }, 500));
    SleepMs(150);
    REQUIRE(g_calls.load() == 1);
    REQUIRE(timeKillEvent(id) == MMSYSERR_INVALPARAM);
}

TEST_CASE("clock timer edge cases", "[clock]")
{
    REQUIRE(timeSetEvent(10, 1, nullptr, 0, TIME_PERIODIC) == 0);
    REQUIRE(timeKillEvent(0) == MMSYSERR_INVALPARAM);
    REQUIRE(timeBeginPeriod(1) == TIMERR_NOERROR);
    REQUIRE(timeEndPeriod(1) == TIMERR_NOERROR);
}

namespace {
std::atomic<int> g_selfCalls { 0 };
std::atomic<MMRESULT> g_selfKill { 99 };
void CALLBACK SelfKillCallback(UINT id, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR)
{
    ++g_selfCalls;
    g_selfKill = timeKillEvent(id);
}
}

TEST_CASE("clock timer may kill itself from its callback", "[clock]")
{
    g_selfCalls = 0;
    MMRESULT id = timeSetEvent(10, 1, SelfKillCallback, 0, TIME_PERIODIC);
    REQUIRE(id != 0);
    REQUIRE(WaitFor([] { return g_selfKill.load() != 99; }, 500));
    SleepMs(100);
    REQUIRE(g_selfKill == TIMERR_NOERROR);
    REQUIRE(g_selfCalls == 1);
}

namespace {
// Only the timer left running below touches this, so the other clock tests'
// counters stay theirs in any order.
std::atomic<int> g_exitTicks { 0 };
void CALLBACK ExitTimerCallback(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR) { ++g_exitTicks; }
}

TEST_CASE("clock running timer is stopped at process exit", "[clock]")
{
    // Deliberately left running; the registry destructor must join it.
    REQUIRE(timeSetEvent(15, 1, ExitTimerCallback, 0, TIME_PERIODIC) != 0);
    REQUIRE(WaitFor([] { return g_exitTicks.load() >= 1; }, 500));
}

namespace {
std::atomic<int> g_oldTicks { 0 };
std::atomic<int> g_newTicks { 0 };
std::atomic<bool> g_restarted { false };
void CALLBACK NewShotCallback(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR) { ++g_newTicks; }
void CALLBACK KillAndRestartCallback(UINT id, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR)
{
    if (++g_oldTicks == 1) {
        timeKillEvent(id);
        timeSetEvent(10, 1, NewShotCallback, 0, TIME_ONESHOT);
        g_restarted = true;
    }
}
}

TEST_CASE("clock timer may kill itself and start another from its callback", "[clock]")
{
    g_oldTicks = 0;
    g_newTicks = 0;
    g_restarted = false;
    REQUIRE(timeSetEvent(10, 1, KillAndRestartCallback, 0, TIME_PERIODIC) != 0);
    REQUIRE(WaitFor([] { return g_newTicks.load() >= 1; }, 1000));
    SleepMs(150);
    REQUIRE(g_restarted);
    REQUIRE(g_newTicks == 1);
    REQUIRE(g_oldTicks == 1);
}

TEST_CASE("clock timeGetDevCaps reports a period range", "[clock]")
{
    TIMECAPS caps = {};
    REQUIRE(timeGetDevCaps(&caps, sizeof(caps)) == TIMERR_NOERROR);
    CHECK(caps.wPeriodMin == 1);
    CHECK(caps.wPeriodMax == 1000000);
    CHECK(timeGetDevCaps(nullptr, sizeof(caps)) == MMSYSERR_INVALPARAM);
    CHECK(timeGetDevCaps(&caps, 1) == MMSYSERR_INVALPARAM);
}

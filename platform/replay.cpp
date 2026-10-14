#define NOMINMAX
#include "platform/replay.h"

#include "compat/msvc_rand.h"
#include "compat/msvc_time.h"
#include "platform/crash.h"
#include "platform/data_dir.h"
#include "platform/present.h"
#include "platform/replay_clock.h"
#include "platform/script.h"
#include "platform/state_dump.h"

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <vector>

namespace platform::replay {

namespace {

    namespace clock = platform::replay_clock;

    // The first timer id, as the recorder hands them out.
    constexpr UINT kFirstTimerId = 0x7100;

    struct Timer {
        UINT id;
        UINT delay;
        LPTIMECALLBACK callback;
        DWORD_PTR user;
        UINT flags;
        uint32_t due;
    };

    struct Config {
        bool active = false;
        std::string dir;
        unsigned dumpEvery = 30;
        unsigned seed = 1;
        replay_probe::Range probe;
    };

    [[noreturn]] void Fail(const std::string& text)
    {
        platform::ReportFatal(text.c_str(), "iwd2r replay");
        std::fflush(stdout);
        std::fflush(stderr);
        std::_Exit(5);
    }

    bool ParseUnsigned(const char* text, unsigned* value)
    {
        if (!text || !*text || *text == '-' || *text == '+')
            return false;
        char* end = nullptr;
        errno = 0;
        const unsigned long long v = std::strtoull(text, &end, 10);
        if (*end != '\0' || errno != 0 || v > 0xFFFFFFFFull)
            return false;
        *value = static_cast<unsigned>(v);
        return true;
    }

    const Config& Settings()
    {
        static const Config config = [] {
            Config c;
            const char* dir = std::getenv("IWD2R_STATE_DUMP");
            if (!dir || !*dir)
                return c;
            c.active = true;
            c.dir = dir;
            while (c.dir.size() > 1 && (c.dir.back() == '/' || c.dir.back() == '\\'))
                c.dir.pop_back();
            const char* every = std::getenv("IWD2R_DUMP_EVERY");
            if (every && *every && !ParseUnsigned(every, &c.dumpEvery))
                Fail(std::string("IWD2R_DUMP_EVERY=") + every + " is not a number");
            const char* seed = std::getenv("IWD2R_SEED");
            if (seed && *seed && !ParseUnsigned(seed, &c.seed))
                Fail(std::string("IWD2R_SEED=") + seed + " is not a number");
            const char* probe = std::getenv("IWD2R_PROBE");
            if (!replay_probe::ParseRange(probe, &c.probe))
                Fail(std::string("IWD2R_PROBE=") + probe + " is not a frame or FIRST-LAST");
            return c;
        }();
        return config;
    }

    // Replay state. g_frame and g_now are read on every thread; the rest
    // belongs to the main thread (OnFrame) unless noted.
    std::atomic<uint32_t> g_frame { 0 };
    std::atomic<uint32_t> g_now { clock::kClockBase };
    std::atomic<DWORD> g_mainThread { 0 };
    uint32_t g_stalledFrames = 0;
    int32_t g_lastAu = 0;
    clock::FrameGate g_gate;
    // The current call of CBaldurChitin::SynchronousUpdate counted as a
    // frame: OnFrameEnd fires its timers.
    bool g_frameCounted = false;
    bool g_haveScript = false;
    bool g_scriptFinished = false;
    // The rand state of the first thread other than the main one to poll
    // input (the AI thread), set once by that thread.
    std::atomic<const unsigned int*> g_inputRandState { nullptr };
    std::atomic<int> g_lastX { 400 };
    std::atomic<int> g_lastY { 300 };

    std::mutex g_timerMutex;
    std::vector<Timer> g_timers;
    UINT g_nextTimerId = kFirstTimerId;

    std::string PathIn(const std::string& name)
    {
        return Settings().dir + "/" + name;
    }

    void Say(const char* format, const std::string& text)
    {
        std::printf("replay: [frame %u] ", g_frame.load());
        std::printf(format, text.c_str());
        std::printf("\n");
        std::fflush(stdout);
    }

    // The time() of the virtual clock, for compat's iwd2r_time.
    bool WallClock(long long* seconds)
    {
        if (!Active())
            return false;
        *seconds = static_cast<long long>(clock::WallClockSeconds(g_now.load()));
        return true;
    }

    const bool g_wallClockInstalled = (iwd2r_set_wall_clock(&WallClock), true);

    state_dump::Snapshot TakeSnapshot(const char* checkpoint)
    {
        state_dump::Snapshot s;
        s.frame = g_frame.load();
        if (checkpoint) {
            s.hasCheckpoint = true;
            s.checkpoint = checkpoint;
        }
        s.rngState = iwd2r_rand_state(); // the main thread's
        s.aiStalledFrames = g_stalledFrames;
        s.loads = g_gate.Loads();
        s.heldFrames = g_gate.HeldFrames();
        if (const unsigned int* ai = g_inputRandState.load()) {
            s.hasAiRngState = true;
            // The AI thread owns this state. It is not proven idle here,
            // but in_async_update is 0 at every checkpoint, so it is not
            // inside its update. The volatile read keeps the compiler from
            // caching or splitting the load; it adds no ordering.
            s.aiRngState = *static_cast<const volatile unsigned int*>(ai);
        }
        if (const state_dump::DumpSource* source = state_dump::Source())
            source->Read(&s.engine);
        return s;
    }

    void WriteDump(const char* checkpoint)
    {
        const std::string name = checkpoint ? "checkpoint-" + std::string(checkpoint) + ".json"
                                            : "frame-" + std::to_string(g_frame.load()) + ".json";
        if (!state_dump::Write(PathIn(name).c_str(), TakeSnapshot(checkpoint)))
            Say("cannot write %s", name);
        else if (checkpoint)
            Say("wrote %s", name);
    }

    std::atomic<ProbeSource> g_probeSource { nullptr };

    // IWD2R_PROBE: the frame's block, appended to probe.txt.
    void WriteProbe(uint32_t frame)
    {
        const ProbeSource source = g_probeSource.load();
        if (!source || !Settings().probe.Has(frame))
            return;
        replay_probe::Frame f;
        f.frame = frame;
        source(&f);
        std::FILE* out = std::fopen(PathIn("probe.txt").c_str(), "ab");
        if (!out) {
            Say("cannot write %s", "probe.txt");
            return;
        }
        const std::string text = replay_probe::Format(f);
        std::fwrite(text.data(), 1, text.size(), out);
        std::fclose(out);
    }

    void WriteStatus()
    {
        const char* state = !g_haveScript ? "no-script" : g_scriptFinished ? "finished"
                                                                           : "running";
        char text[160];
        std::snprintf(text, sizeof(text), "frame=%u\nscript_line=%d\nstate=%s\n", g_frame.load(),
            g_haveScript ? platform::script::Line() : 0, state);
        state_dump::WriteFileAtomically(PathIn("status.txt"), text);
    }

    void FireTimers()
    {
        std::vector<Timer> due;
        {
            std::lock_guard<std::mutex> lock(g_timerMutex);
            const uint32_t now = g_now.load();
            for (size_t i = 0; i < g_timers.size();) {
                Timer& t = g_timers[i];
                if (static_cast<int32_t>(now - t.due) < 0) {
                    ++i;
                    continue;
                }
                due.push_back(t);
                if (t.flags & TIME_PERIODIC) {
                    t.due = now + t.delay; // at most once per frame
                    ++i;
                } else {
                    g_timers.erase(g_timers.begin() + static_cast<std::ptrdiff_t>(i));
                }
            }
        }
        for (const Timer& t : due)
            t.callback(t.id, 0, t.user, 0, 0);
    }

    void NoteInputThread()
    {
        const DWORD mainThread = g_mainThread.load();
        if (mainThread != 0 && GetCurrentThreadId() != mainThread && !g_inputRandState.load()) {
            g_inputRandState = iwd2r_rand_state_address();
            std::printf("replay: input polled on thread %lu\n", static_cast<unsigned long>(GetCurrentThreadId()));
            std::fflush(stdout);
        }
    }

}

bool Active()
{
    return Settings().active;
}

uint32_t NowMs()
{
    return g_now.load();
}

uint32_t Frame()
{
    return g_frame.load();
}

void Checkpoint(const std::string& name, int line)
{
    if (!Active())
        return;
    Say("checkpoint %s", name + " (line " + std::to_string(line) + ")");
    WriteDump(name.c_str());
}

namespace {

    // The recorder's IsFrame: the frame gate's question for this call.
    bool IsFrame(const state_dump::DumpSource* source)
    {
        if (g_frame.load() == 0 || !source || !source->Ready())
            return true;
        state_dump::AiState ai;
        source->ReadAi(&ai);
        const uint32_t loadsBefore = g_gate.Loads();
        if (!g_gate.Counts(ai.auCounter, ai.inAsyncUpdate != 0, ai.loading != 0)) {
            if (g_gate.Stretch() == 1)
                Say("frames held: drawn inside AI update %s",
                    std::to_string(ai.auCounter) + (ai.loading ? " (progress bar up)" : " (progress bar down)"));
            else if (g_gate.Stretch() == 0)
                Say("frame held: the redraw the end of AI update %s asked for, after a frame already counted for it",
                    std::to_string(ai.auCounter));
            if (g_gate.Loads() != loadsBefore)
                Say("load %s", std::to_string(g_gate.Loads()) + ": AI update " + std::to_string(ai.auCounter));
            return false;
        }
        if (g_gate.EndedStretch())
            Say("%s", std::to_string(g_gate.EndedStretch()) + " calls held inside the last AI update; frames go on (au_counter " + std::to_string(ai.auCounter) + ")");
        return true;
    }

}

// The recorder's OnFrame, step for step.
void OnFrame()
{
    platform::crash::MaybeTestCrash();
    if (!Active())
        return;
    (void)g_wallClockInstalled;
    const state_dump::DumpSource* source = state_dump::Source();
    if (!IsFrame(source))
        return;
    const uint32_t frame = ++g_frame;
    iwd2r_rand_trace_frame(frame);
    if (frame == 1) {
        g_mainThread = GetCurrentThreadId();
        iwd2r_srand(Settings().seed);
        Say("first frame; srand(%s) on the main thread", std::to_string(Settings().seed));
        const SIZE screen = platform::present::ScreenSize();
        g_haveScript = platform::script::StartPending(static_cast<int>(screen.cx), static_cast<int>(screen.cy));
        g_lastX = static_cast<int>(screen.cx) / 2;
        g_lastY = static_cast<int>(screen.cy) / 2;
        if (g_haveScript)
            Say("scenario started on a %s screen",
                std::to_string(screen.cx) + "x" + std::to_string(screen.cy));
    }
    if (g_haveScript && !g_scriptFinished) {
        if (platform::script::Advance() || !platform::script::Running()) {
            g_scriptFinished = true;
            Say("scenario finished%s", "");
        }
    }
    {
        // As the recorder: a frame with no AI update since the previous one
        // and none in progress is a stall.
        if (source && source->Ready()) {
            state_dump::AiState ai;
            source->ReadAi(&ai);
            if (ai.auCounter == g_lastAu && ai.inAsyncUpdate == 0) {
                if (g_stalledFrames++ == 0)
                    Say("AI stall: no AI update since the previous frame (au_counter %s)",
                        std::to_string(ai.auCounter));
            }
            g_lastAu = ai.auCounter;
        }
    }
    const unsigned every = Settings().dumpEvery;
    if (every && frame % every == 0)
        WriteDump(nullptr);
    WriteProbe(frame);
    WriteStatus();
    g_now = clock::TickAfterFrame(frame);
    g_frameCounted = true;
}

void SetProbeSource(ProbeSource source)
{
    g_probeSource = source;
}

// The recorder's OnFrameEnd: the timers of a frame that counted fire once
// the frame is drawn.
void OnFrameEnd()
{
    if (!Active() || !g_frameCounted)
        return;
    g_frameCounted = false;
    FireTimers();
}

MMRESULT SetTimer(UINT delayMs, LPTIMECALLBACK callback, DWORD_PTR user, UINT flags)
{
    std::lock_guard<std::mutex> lock(g_timerMutex);
    Timer t = { g_nextTimerId++, delayMs, callback, user, flags, g_now.load() + delayMs };
    g_timers.push_back(t);
    std::printf("replay: timeSetEvent(%u ms, %s) = %u, driven by the virtual clock\n", delayMs,
        (flags & TIME_PERIODIC) ? "periodic" : "one shot", t.id);
    std::fflush(stdout);
    return t.id;
}

bool KillTimer(UINT id)
{
    std::lock_guard<std::mutex> lock(g_timerMutex);
    for (auto it = g_timers.begin(); it != g_timers.end(); ++it) {
        if (it->id == id) {
            g_timers.erase(it);
            return true;
        }
    }
    return false;
}

POINT Cursor()
{
    NoteInputThread();
    POINT point = {};
    if (platform::script::InjectedCursor(&point)) {
        g_lastX = static_cast<int>(point.x);
        g_lastY = static_cast<int>(point.y);
    }
    point.x = g_lastX.load();
    point.y = g_lastY.load();
    return point;
}

SHORT Key(int vk)
{
    NoteInputThread();
    SHORT state = 0;
    return platform::script::InjectedKey(vk, &state) ? state : SHORT(0);
}

void SetCursor(int x, int y)
{
    g_lastX = x;
    g_lastY = y;
}

}

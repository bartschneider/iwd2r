// iwd2r. Distributed under LICENSE.md.
#define NOMINMAX
#include "platform/clock.h"
#include "platform/win32_api.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace platform {

ClockSource::~ClockSource() = default;

namespace {

    using SteadyClock = std::chrono::steady_clock;

    class DefaultSource : public ClockSource {
    public:
        uint64_t NowMs() override
        {
            static const SteadyClock::time_point start = SteadyClock::now();
            return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(SteadyClock::now() - start).count());
        }
    };

    DefaultSource& DefaultInstance()
    {
        // Immortal on purpose: timer threads may still read the clock while the
        // timer registry is being torn down during static destruction.
        static DefaultSource* instance = new DefaultSource;
        return *instance;
    }

    std::atomic<ClockSource*>& CurrentSource()
    {
        static std::atomic<ClockSource*> current { nullptr };
        return current;
    }

} // namespace

ClockSource* DefaultClockSource()
{
    return &DefaultInstance();
}

void SetClockSource(ClockSource* source)
{
    CurrentSource().store(source);
}

} // namespace platform

namespace {

struct Timer {
    UINT id = 0;
    UINT delayMs = 0;
    UINT flags = 0;
    LPTIMECALLBACK callback = nullptr;
    DWORD_PTR user = 0;
    std::thread thread;
    std::mutex lock;
    std::condition_variable cv;
    bool stop = false;
};

class TimerRegistry {
public:
    ~TimerRegistry() { StopAll(); }

    MMRESULT Start(UINT delayMs, LPTIMECALLBACK callback, DWORD_PTR user, UINT flags)
    {
        auto timer = std::make_shared<Timer>();
        std::vector<std::shared_ptr<Timer>> finished;
        {
            std::lock_guard<std::mutex> guard(m_lock);
            if (m_shuttingDown)
                return 0;
            timer->id = m_nextId++;
            timer->delayMs = delayMs;
            timer->flags = flags;
            timer->callback = callback;
            timer->user = user;
            finished.swap(m_retired);
            m_timers[timer->id] = timer;
            timer->thread = std::thread([this, timer] { Run(timer); });
        }
        Join(finished);
        return timer->id;
    }

    MMRESULT Kill(UINT id)
    {
        std::shared_ptr<Timer> timer;
        {
            std::lock_guard<std::mutex> guard(m_lock);
            auto it = m_timers.find(id);
            if (it == m_timers.end())
                return MMSYSERR_INVALPARAM;
            timer = it->second;
            m_timers.erase(it);
            if (timer->thread.get_id() == std::this_thread::get_id()) {
                // Called from the timer's own callback: cannot join ourselves.
                RequestStop(*timer);
                m_retired.push_back(timer);
                return TIMERR_NOERROR;
            }
        }
        RequestStop(*timer);
        if (timer->thread.joinable())
            timer->thread.join();
        return TIMERR_NOERROR;
    }

private:
    static void RequestStop(Timer& timer)
    {
        std::lock_guard<std::mutex> guard(timer.lock);
        timer.stop = true;
        timer.cv.notify_all();
    }

    // Joins the given timers, except the calling thread's own timer, which is
    // handed back to the retired list to be joined by someone else.
    void Join(std::vector<std::shared_ptr<Timer>>& timers)
    {
        std::vector<std::shared_ptr<Timer>> own;
        for (auto& timer : timers) {
            if (timer->thread.get_id() == std::this_thread::get_id())
                own.push_back(timer);
            else if (timer->thread.joinable())
                timer->thread.join();
        }
        timers.clear();
        if (!own.empty()) {
            std::lock_guard<std::mutex> guard(m_lock);
            for (auto& timer : own)
                m_retired.push_back(timer);
        }
    }

    void StopAll()
    {
        std::vector<std::shared_ptr<Timer>> all;
        {
            std::lock_guard<std::mutex> guard(m_lock);
            m_shuttingDown = true;
            for (auto& entry : m_timers)
                all.push_back(entry.second);
            m_timers.clear();
            for (auto& timer : m_retired)
                all.push_back(timer);
            m_retired.clear();
        }
        for (auto& timer : all)
            RequestStop(*timer);
        Join(all);
    }

    // Removes a one-shot timer once it has fired; its id becomes unusable.
    void Retire(const std::shared_ptr<Timer>& timer)
    {
        std::lock_guard<std::mutex> guard(m_lock);
        if (m_timers.erase(timer->id) != 0)
            m_retired.push_back(timer);
    }

    void Run(const std::shared_ptr<Timer>& timer)
    {
        const auto delay = std::chrono::milliseconds(timer->delayMs ? timer->delayMs : 1);
        const auto start = std::chrono::steady_clock::now();
        std::int64_t n = 1;
        for (;;) {
            const auto due = start + delay * n;
            {
                std::unique_lock<std::mutex> guard(timer->lock);
                if (timer->cv.wait_until(guard, due, [&] { return timer->stop; }))
                    return;
            }
            timer->callback(timer->id, 0, timer->user, 0, 0);
            if (!(timer->flags & TIME_PERIODIC)) {
                Retire(timer);
                return;
            }
            {
                std::lock_guard<std::mutex> guard(timer->lock);
                if (timer->stop)
                    return;
            }
            ++n;
        }
    }

    std::mutex m_lock;
    std::map<UINT, std::shared_ptr<Timer>> m_timers;
    std::vector<std::shared_ptr<Timer>> m_retired;
    UINT m_nextId = 1;
    bool m_shuttingDown = false;
};

TimerRegistry& Registry()
{
    static TimerRegistry registry;
    return registry;
}

DWORD TickCount()
{
    platform::ClockSource* source = platform::CurrentSource().load();
    if (!source)
        source = platform::DefaultClockSource();
    return static_cast<DWORD>(source->NowMs());
}

} // namespace

DWORD GetTickCount()
{
    return TickCount();
}

DWORD timeGetTime()
{
    return TickCount();
}

void Sleep(DWORD milliseconds)
{
    if (milliseconds == 0)
        std::this_thread::yield();
    else
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

DWORD SleepEx(DWORD milliseconds, BOOL)
{
    Sleep(milliseconds);
    return 0;
}

MMRESULT timeSetEvent(UINT delayMs, UINT, LPTIMECALLBACK callback, DWORD_PTR user, UINT flags)
{
    if (!callback)
        return 0;
    return Registry().Start(delayMs, callback, user, flags);
}

MMRESULT timeKillEvent(UINT id)
{
    return Registry().Kill(id);
}

MMRESULT timeGetDevCaps(LPTIMECAPS caps, UINT size)
{
    if (!caps || size < sizeof(TIMECAPS))
        return MMSYSERR_INVALPARAM;
    caps->wPeriodMin = 1;
    caps->wPeriodMax = 1000000;
    return TIMERR_NOERROR;
}

MMRESULT timeBeginPeriod(UINT)
{
    return TIMERR_NOERROR;
}

MMRESULT timeEndPeriod(UINT)
{
    return TIMERR_NOERROR;
}

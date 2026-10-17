#define NOMINMAX
// Order is deliberate: win32_api.h before handles.h (after NOMINMAX).
// clang-format off
#include "platform/win32_api.h"
#include "platform/handles.h"
// clang-format on
#include "platform/affinity.h"
#include "platform/replay.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <system_error>
#include <thread>

namespace {

struct ThreadState {
    std::mutex lock;
    std::condition_variable cv;
    bool done = false;
    // SuspendThread is cooperative: only a thread can suspend itself (the
    // engine's worker threads park this way until ResumeThread).
    DWORD suspendCount = 0;
};

// The state of the calling thread: set by _beginthread for its threads, made
// on first use for any other thread (such as the main thread).
thread_local std::shared_ptr<ThreadState> t_threadState;

std::shared_ptr<ThreadState> CurrentThreadState()
{
    if (!t_threadState)
        t_threadState = std::make_shared<ThreadState>();
    return t_threadState;
}

// Win32 pseudo handles, which CloseHandle ignores.
HANDLE const kCurrentProcess = reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1));
HANDLE const kCurrentThread = reinterpret_cast<HANDLE>(static_cast<intptr_t>(-2));

struct HandleObject {
    enum class Kind { Thread,
        Event,
        Semaphore,
        Mutex,
        File } kind;

    explicit HandleObject(Kind k)
        : kind(k)
    {
    }

    // Thread
    std::shared_ptr<ThreadState> threadState;
    std::thread thread;
    std::mutex joinLock;

    // Event and semaphore share one lock and condition variable.
    std::mutex lock;
    std::condition_variable cv;
    bool signaled = false;
    bool manualReset = false;
    // iwd2r: the engine's named AI-thread timer event (see WaitForSingleObject).
    bool replayTimer = false;
    // iwd2r: CloseHandle may run while a waiter is still inside the waits
    // below (CChitin::~CChitin 0x7923B0 closes m_eventTimer). Destroying a
    // condition variable with a waiter in it aborts the process, so a
    // handle closed under a waiter is only marked here, and the waiter
    // that finds waiters back at 0 deletes the object.
    uint32_t waiters = 0;
    bool closed = false;
    LONG count = 0;
    LONG maximum = 0;

    // Mutex
    std::recursive_timed_mutex mutex;

    // File
    std::FILE* file = nullptr;
};

HandleObject* toObject(HANDLE handle) { return static_cast<HandleObject*>(handle); }

template <typename Pred>
bool waitFor(std::unique_lock<std::mutex>& guard, std::condition_variable& cv, DWORD ms, Pred pred)
{
    if (ms == INFINITE) {
        cv.wait(guard, pred);
        return true;
    }
    return cv.wait_for(guard, std::chrono::milliseconds(ms), pred);
}

}

uintptr_t _beginthread(void (*start)(void*), unsigned, void* arg)
{
    HandleObject* object = nullptr;
    try {
        object = new HandleObject(HandleObject::Kind::Thread);
        object->threadState = std::make_shared<ThreadState>();
        std::shared_ptr<ThreadState> state = object->threadState;
        object->thread = std::thread([state, start, arg] {
            t_threadState = state;
            start(arg);
            {
                std::lock_guard<std::mutex> guard(state->lock);
                state->done = true;
            }
            state->cv.notify_all();
        });
    } catch (const std::bad_alloc&) {
        delete object;
        return static_cast<uintptr_t>(-1);
    } catch (const std::system_error&) {
        delete object;
        return static_cast<uintptr_t>(-1);
    }
    return reinterpret_cast<uintptr_t>(object);
}

DWORD GetCurrentThreadId()
{
    static std::atomic<DWORD> next { 1 };
    thread_local DWORD id = next.fetch_add(1);
    return id;
}

DWORD WaitForSingleObject(HANDLE handle, DWORD milliseconds)
{
    if (!handle || handle == kCurrentProcess || handle == kCurrentThread)
        return WAIT_TIMEOUT;
    HandleObject* object = toObject(handle);
    switch (object->kind) {
    case HandleObject::Kind::Thread: {
        ThreadState& state = *object->threadState;
        std::unique_lock<std::mutex> guard(state.lock);
        if (!waitFor(guard, state.cv, milliseconds, [&] { return state.done; }))
            return WAIT_TIMEOUT;
        guard.unlock();
        std::lock_guard<std::mutex> join(object->joinLock);
        if (object->thread.joinable())
            object->thread.join();
        return WAIT_OBJECT_0;
    }
    case HandleObject::Kind::Event: {
        std::unique_lock<std::mutex> guard(object->lock);
        ++object->waiters;
        // Leaving the wait: drop the waiter count and, when CloseHandle
        // marked the object closed under us and we are the last waiter,
        // destroy it -- after the lock is gone, never inside the wait.
        const auto leave = [&](bool signaled) {
            --object->waiters;
            const bool doomed = object->closed && object->waiters == 0;
            guard.unlock();
            if (doomed)
                delete object;
            return signaled ? WAIT_OBJECT_0 : WAIT_TIMEOUT;
        };
        if (object->replayTimer && milliseconds != INFINITE && platform::replay::Active()) {
            // iwd2r: the engine's AI thread waits on the BaldurTimerEvent
            // for up to 100 ms (CBaldurChitin::MainAIThread 0x4242B0; the
            // two 100 ms waits sit at 0x4244B7 and 0x4244D8). The
            // replay mode sets that event at frame ends on the virtual
            // clock (platform/replay.cpp FireTimers), so the timeout is
            // measured on that clock too: a frame slowed by machine load
            // must not let the wait expire and start the AI update before
            // the render ends (the ai_overlap_frames invariant). Without a
            // replay the wait is exactly the asked-for real time.
            //
            // iwd2r: the virtual clock stops for good after the last frame
            // (the scenario is over; nothing advances g_now any more), so
            // a virtual deadline in that state never arrives: the wait --
            // and with it the engine's whole exit -- would hang forever
            // (the intermittent world_entry INJECTOR_TIMEOUT). While the
            // AI thread sits in this wait, a frame count frozen for 2 s of
            // real time means no frame will come again: a frame still
            // being rendered counts at its start (platform/replay.cpp
            // OnFrame) and keeps the count moving, and the renders the
            // M2-26 property protects are far shorter than 2 s even at a
            // 10x load (a violation would be counted in ai_overlap_frames
            // and fail the compare, not pass silently). On the fallback
            // the wait times out on the real clock, the loop above sees
            // the exit flag and the thread parks. The fallback is armed
            // only after the first frame: before it, the original's AI
            // thread does run early updates on its 100 ms real timeout,
            // and ours must not.
            const uint32_t deadline = platform::replay::NowMs() + milliseconds;
            uint32_t frame = platform::replay::Frame();
            std::chrono::steady_clock::time_point froze = std::chrono::steady_clock::now();
            while (!object->signaled) {
                if (platform::replay::NowMs() >= deadline)
                    return leave(false);
                if (const uint32_t now = platform::replay::Frame(); now != frame) {
                    frame = now;
                    froze = std::chrono::steady_clock::now();
                } else if (frame != 0 && std::chrono::steady_clock::now() - froze > std::chrono::seconds(2)) {
                    return leave(false);
                }
                object->cv.wait_for(guard, std::chrono::milliseconds(milliseconds),
                    [&] { return object->signaled; });
            }
        } else if (!waitFor(guard, object->cv, milliseconds, [&] { return object->signaled; })) {
            return leave(false);
        }
        if (!object->manualReset)
            object->signaled = false;
        return leave(true);
    }
    case HandleObject::Kind::Semaphore: {
        std::unique_lock<std::mutex> guard(object->lock);
        if (!waitFor(guard, object->cv, milliseconds, [&] { return object->count > 0; }))
            return WAIT_TIMEOUT;
        --object->count;
        return WAIT_OBJECT_0;
    }
    case HandleObject::Kind::Mutex:
        if (milliseconds == INFINITE) {
            object->mutex.lock();
            return WAIT_OBJECT_0;
        }
        return object->mutex.try_lock_for(std::chrono::milliseconds(milliseconds)) ? WAIT_OBJECT_0 : WAIT_TIMEOUT;
    case HandleObject::Kind::File:
        return WAIT_OBJECT_0;
    }
    return WAIT_TIMEOUT;
}

BOOL CloseHandle(HANDLE handle)
{
    if (!handle || handle == INVALID_HANDLE_VALUE)
        return FALSE;
    if (handle == kCurrentThread)
        return TRUE;
    HandleObject* object = toObject(handle);
    if (object->kind == HandleObject::Kind::Thread) {
        std::lock_guard<std::mutex> join(object->joinLock);
        if (object->thread.joinable())
            object->thread.detach();
    }
    if (object->kind == HandleObject::Kind::File && object->file)
        std::fclose(object->file);
    if (object->kind == HandleObject::Kind::Event) {
        // iwd2r: CChitin::~CChitin (0x7923B0) closes m_eventTimer while the
        // AI thread can still be inside its 100 ms wait (the replay clock's
        // post-scenario freeze delayed it past WaitForMainAIThread's 10 s).
        // Destroying the event's condition variable under that waiter
        // aborts the process (pthread_cond_destroy with a waiter); mark it
        // and let the last waiter out delete it (see WaitForSingleObject).
        std::lock_guard<std::mutex> guard(object->lock);
        if (object->waiters > 0) {
            object->closed = true;
            return TRUE;
        }
    }
    delete object;
    return TRUE;
}

HANDLE GetCurrentProcess() { return kCurrentProcess; }

HANDLE GetCurrentThread() { return kCurrentThread; }

// Nonzero and fixed; the engine only passes it to OpenProcess, which is inert.
DWORD GetCurrentProcessId() { return 1; }

BOOL SetProcessAffinityMask(HANDLE process, DWORD_PTR mask)
{
    if (process != NULL && process != kCurrentProcess) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    if (!platform::SetNativeProcessAffinity(static_cast<uint64_t>(mask))) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    return TRUE;
}

BOOL DuplicateHandle(HANDLE sourceProcess, HANDLE source, HANDLE targetProcess, HANDLE* target, DWORD, BOOL, DWORD)
{
    // Only the engine's use is supported: a real handle for the calling
    // thread, made from the GetCurrentThread pseudo handle.
    if (!target || sourceProcess != kCurrentProcess || targetProcess != kCurrentProcess || source != kCurrentThread) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    HandleObject* object = new HandleObject(HandleObject::Kind::Thread);
    object->threadState = CurrentThreadState();
    *target = object;
    return TRUE;
}

namespace {

ThreadState* StateOf(HANDLE thread)
{
    if (thread == kCurrentThread)
        return CurrentThreadState().get();
    if (!thread || thread == kCurrentProcess || thread == INVALID_HANDLE_VALUE)
        return nullptr;
    HandleObject* object = toObject(thread);
    return object->kind == HandleObject::Kind::Thread ? object->threadState.get() : nullptr;
}

}

DWORD SuspendThread(HANDLE thread)
{
    ThreadState* state = StateOf(thread);
    if (!state)
        return static_cast<DWORD>(-1);
    std::unique_lock<std::mutex> guard(state->lock);
    const DWORD previous = state->suspendCount;
    // Another thread cannot be stopped portably: report its count, change nothing.
    if (state != CurrentThreadState().get())
        return previous;
    ++state->suspendCount;
    state->cv.wait(guard, [state] { return state->suspendCount == 0; });
    return previous;
}

DWORD ResumeThread(HANDLE thread)
{
    ThreadState* state = StateOf(thread);
    if (!state)
        return static_cast<DWORD>(-1);
    DWORD previous;
    {
        std::lock_guard<std::mutex> guard(state->lock);
        previous = state->suspendCount;
        if (state->suspendCount > 0)
            --state->suspendCount;
    }
    state->cv.notify_all();
    return previous;
}

int GetThreadPriority(HANDLE thread)
{
    return StateOf(thread) ? THREAD_PRIORITY_NORMAL : THREAD_PRIORITY_ERROR_RETURN;
}

BOOL SetThreadPriority(HANDLE thread, int)
{
    return StateOf(thread) ? TRUE : FALSE;
}

namespace platform::handles {

HANDLE MakeFileHandle(std::FILE* file)
{
    HandleObject* object = new HandleObject(HandleObject::Kind::File);
    object->file = file;
    return object;
}

std::FILE* FileOf(HANDLE handle)
{
    if (!handle || handle == INVALID_HANDLE_VALUE || handle == kCurrentProcess || handle == kCurrentThread)
        return nullptr;
    HandleObject* object = toObject(handle);
    return object->kind == HandleObject::Kind::File ? object->file : nullptr;
}

}

HANDLE CreateEventA(LPVOID, BOOL manualReset, BOOL initialState, LPCSTR name)
{
    HandleObject* object = new HandleObject(HandleObject::Kind::Event);
    object->manualReset = manualReset != FALSE;
    object->signaled = initialState != FALSE;
    // iwd2r: CChitin's ctor makes the AI thread's timer event under this
    // name (CreateEventA call 0x7901A8, name string 0x8BA024, handle
    // stored to the field MainAIThread waits on at 0x4244A6); only that
    // event's waits follow the replay clock.
    object->replayTimer = name != nullptr && std::strcmp(name, "BaldurTimerEvent") == 0;
    return object;
}

BOOL SetEvent(HANDLE event)
{
    if (!event)
        return FALSE;
    HandleObject* object = toObject(event);
    {
        std::lock_guard<std::mutex> guard(object->lock);
        object->signaled = true;
    }
    if (object->manualReset)
        object->cv.notify_all();
    else
        object->cv.notify_one();
    return TRUE;
}

BOOL ResetEvent(HANDLE event)
{
    if (!event)
        return FALSE;
    HandleObject* object = toObject(event);
    std::lock_guard<std::mutex> guard(object->lock);
    object->signaled = false;
    return TRUE;
}

HANDLE CreateSemaphoreA(LPVOID, LONG initial, LONG maximum, LPCSTR)
{
    if (maximum <= 0 || initial < 0 || initial > maximum)
        return nullptr;
    HandleObject* object = new HandleObject(HandleObject::Kind::Semaphore);
    object->count = initial;
    object->maximum = maximum;
    return object;
}

BOOL ReleaseSemaphore(HANDLE semaphore, LONG count, LONG* previous)
{
    if (!semaphore || count <= 0)
        return FALSE;
    HandleObject* object = toObject(semaphore);
    {
        std::lock_guard<std::mutex> guard(object->lock);
        if (object->count > object->maximum - count)
            return FALSE;
        if (previous)
            *previous = object->count;
        object->count += count;
    }
    object->cv.notify_all();
    return TRUE;
}

HANDLE CreateMutexA(LPVOID, BOOL initialOwner, LPCSTR)
{
    HandleObject* object = new HandleObject(HandleObject::Kind::Mutex);
    if (initialOwner)
        object->mutex.lock();
    return object;
}

BOOL ReleaseMutex(HANDLE mutex)
{
    if (!mutex)
        return FALSE;
    toObject(mutex)->mutex.unlock();
    return TRUE;
}

void InitializeCriticalSection(CRITICAL_SECTION* section)
{
    section->impl = new std::recursive_mutex;
}

void EnterCriticalSection(CRITICAL_SECTION* section)
{
    static_cast<std::recursive_mutex*>(section->impl)->lock();
}

void LeaveCriticalSection(CRITICAL_SECTION* section)
{
    static_cast<std::recursive_mutex*>(section->impl)->unlock();
}

void DeleteCriticalSection(CRITICAL_SECTION* section)
{
    delete static_cast<std::recursive_mutex*>(section->impl);
    section->impl = nullptr;
}

BOOL TryEnterCriticalSection(CRITICAL_SECTION* section)
{
    return static_cast<std::recursive_mutex*>(section->impl)->try_lock() ? TRUE : FALSE;
}

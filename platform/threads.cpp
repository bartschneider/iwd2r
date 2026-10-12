// iwd2r. Distributed under LICENSE.md.
#define NOMINMAX
#include "platform/handles.h"
#include "platform/win32_api.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
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
        if (!waitFor(guard, object->cv, milliseconds, [&] { return object->signaled; }))
            return WAIT_TIMEOUT;
        if (!object->manualReset)
            object->signaled = false;
        return WAIT_OBJECT_0;
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
    delete object;
    return TRUE;
}

HANDLE GetCurrentProcess() { return kCurrentProcess; }

HANDLE GetCurrentThread() { return kCurrentThread; }

// Nonzero and fixed; the engine only passes it to OpenProcess, which is inert.
DWORD GetCurrentProcessId() { return 1; }

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

HANDLE CreateEventA(LPVOID, BOOL manualReset, BOOL initialState, LPCSTR)
{
    HandleObject* object = new HandleObject(HandleObject::Kind::Event);
    object->manualReset = manualReset != FALSE;
    object->signaled = initialState != FALSE;
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

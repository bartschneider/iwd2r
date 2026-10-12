// iwd2r. Distributed under LICENSE.md.
#define NOMINMAX
#include "platform/win32_api.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <memory>
#include <new>
#include <system_error>
#include <thread>

namespace {

struct ThreadState {
    std::mutex lock;
    std::condition_variable cv;
    bool done = false;
};

struct HandleObject {
    enum class Kind { Thread, Event, Semaphore, Mutex } kind;

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
    if (!handle)
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
    }
    return WAIT_TIMEOUT;
}

BOOL CloseHandle(HANDLE handle)
{
    if (!handle)
        return FALSE;
    HandleObject* object = toObject(handle);
    if (object->kind == HandleObject::Kind::Thread) {
        std::lock_guard<std::mutex> join(object->joinLock);
        if (object->thread.joinable())
            object->thread.detach();
    }
    delete object;
    return TRUE;
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

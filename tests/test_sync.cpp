#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "compat/mfc_sync.h"
#include "platform/win32_api.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace {

// Polls until the flag is set or 500 ms have passed.
bool waitForFlag(const std::atomic<bool>& flag)
{
    for (int i = 0; i < 500 && !flag.load(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return flag.load();
}

void threadBody(void* arg)
{
    static_cast<std::atomic<int>*>(arg)->store(42);
}

}

TEST_CASE("sync: CCriticalSection locks recursively", "[sync]")
{
    CCriticalSection cs;
    REQUIRE(cs.Lock() == TRUE);
    REQUIRE(cs.Lock() == TRUE);
    REQUIRE(cs.Unlock() == TRUE);
    REQUIRE(cs.Unlock() == TRUE);
}

TEST_CASE("sync: CSingleLock releases on destruction", "[sync]")
{
    CCriticalSection cs;
    {
        CSingleLock lock(&cs, TRUE);
        REQUIRE(lock.IsLocked() == TRUE);
    }
    std::atomic<bool> got { false };
    std::thread t([&] {
        cs.Lock();
        got = true;
        cs.Unlock();
    });
    bool ok = waitForFlag(got);
    t.join();
    REQUIRE(ok);

    CSingleLock unlocked(&cs);
    REQUIRE(unlocked.IsLocked() == FALSE);
    REQUIRE(unlocked.Lock() == TRUE);
    REQUIRE(unlocked.IsLocked() == TRUE);
    REQUIRE(unlocked.Unlock() == TRUE);
    REQUIRE(unlocked.IsLocked() == FALSE);
}

TEST_CASE("sync: CEvent and CMutex through CSingleLock", "[sync]")
{
    CEvent manual(TRUE, TRUE);
    CSingleLock l1(&manual);
    REQUIRE(l1.Lock(50) == TRUE);
    CSingleLock l2(&manual);
    REQUIRE(l2.Lock(50) == TRUE);
    manual.ResetEvent();
    CSingleLock l3(&manual);
    REQUIRE(l3.Lock(50) == FALSE);

    CEvent autoEvent(TRUE, FALSE);
    CSingleLock a1(&autoEvent);
    REQUIRE(a1.Lock(50) == TRUE);
    CSingleLock a2(&autoEvent);
    REQUIRE(a2.Lock(50) == FALSE);

    CMutex mutex;
    CSingleLock m1(&mutex, TRUE);
    CSingleLock m2(&mutex, TRUE);
    REQUIRE(m1.IsLocked() == TRUE);
    REQUIRE(m2.IsLocked() == TRUE);
}

TEST_CASE("sync: manual-reset event stays signaled", "[sync]")
{
    HANDLE h = CreateEventA(nullptr, TRUE, TRUE, nullptr);
    REQUIRE(h != nullptr);
    REQUIRE(WaitForSingleObject(h, 0) == WAIT_OBJECT_0);
    REQUIRE(WaitForSingleObject(h, 0) == WAIT_OBJECT_0);
    REQUIRE(ResetEvent(h) == TRUE);
    REQUIRE(WaitForSingleObject(h, 50) == WAIT_TIMEOUT);
    REQUIRE(CloseHandle(h) == TRUE);
    REQUIRE(CloseHandle(nullptr) == FALSE);
}

TEST_CASE("sync: auto-reset event is consumed by a wait", "[sync]")
{
    HANDLE h = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    REQUIRE(SetEvent(h) == TRUE);
    REQUIRE(WaitForSingleObject(h, 50) == WAIT_OBJECT_0);
    REQUIRE(WaitForSingleObject(h, 50) == WAIT_TIMEOUT);
    CloseHandle(h);
}

TEST_CASE("sync: SetEvent from another thread wakes a waiter", "[sync]")
{
    HANDLE h = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    std::atomic<DWORD> result { 0xdead };
    std::thread waiter([&] { result = WaitForSingleObject(h, 500); });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    SetEvent(h);
    waiter.join();
    REQUIRE(result.load() == WAIT_OBJECT_0);
    CloseHandle(h);
}

TEST_CASE("sync: semaphore counts", "[sync]")
{
    HANDLE h = CreateSemaphoreA(nullptr, 0, 10, nullptr);
    REQUIRE(h != nullptr);
    REQUIRE(WaitForSingleObject(h, 50) == WAIT_TIMEOUT);
    LONG prev = -1;
    REQUIRE(ReleaseSemaphore(h, 1, &prev) == TRUE);
    REQUIRE(prev == 0);
    REQUIRE(WaitForSingleObject(h, 50) == WAIT_OBJECT_0);
    REQUIRE(WaitForSingleObject(h, 50) == WAIT_TIMEOUT);
    REQUIRE(ReleaseSemaphore(h, 10, nullptr) == TRUE);
    REQUIRE(ReleaseSemaphore(h, 1, &prev) == FALSE);
    CloseHandle(h);
}

TEST_CASE("sync: mutex handle is recursive and distinct per call", "[sync]")
{
    HANDLE a = CreateMutexA(nullptr, TRUE, nullptr);
    HANDLE b = CreateMutexA(nullptr, FALSE, nullptr);
    REQUIRE(a != nullptr);
    REQUIRE(a != b);
    REQUIRE(WaitForSingleObject(a, 50) == WAIT_OBJECT_0);
    std::atomic<DWORD> other { 0xdead };
    std::thread t([&] { other = WaitForSingleObject(a, 50); });
    t.join();
    REQUIRE(other.load() == WAIT_TIMEOUT);
    CloseHandle(a);
    CloseHandle(b);
}

TEST_CASE("sync: _beginthread runs and can be joined", "[sync]")
{
    std::atomic<int> value { 0 };
    uintptr_t r = _beginthread(threadBody, 0, &value);
    REQUIRE(r != static_cast<uintptr_t>(-1));
    HANDLE h = reinterpret_cast<HANDLE>(r);
    REQUIRE(WaitForSingleObject(h, INFINITE) == WAIT_OBJECT_0);
    REQUIRE(value.load() == 42);
    REQUIRE(WaitForSingleObject(h, INFINITE) == WAIT_OBJECT_0);
    REQUIRE(CloseHandle(h) == TRUE);
}

TEST_CASE("sync: CRITICAL_SECTION is recursive and excludes other threads", "[sync]")
{
    CRITICAL_SECTION cs;
    InitializeCriticalSection(&cs);
    EnterCriticalSection(&cs);
    EnterCriticalSection(&cs);
    std::atomic<int> other { -1 };
    std::thread t([&] { other = TryEnterCriticalSection(&cs); });
    t.join();
    REQUIRE(other.load() == FALSE);
    LeaveCriticalSection(&cs);
    LeaveCriticalSection(&cs);
    std::thread t2([&] {
        other = TryEnterCriticalSection(&cs);
        if (other.load())
            LeaveCriticalSection(&cs);
    });
    t2.join();
    REQUIRE(other.load() == TRUE);
    DeleteCriticalSection(&cs);
}

TEST_CASE("sync: GetCurrentThreadId is nonzero and per thread", "[sync]")
{
    DWORD mine = GetCurrentThreadId();
    REQUIRE(mine != 0);
    REQUIRE(GetCurrentThreadId() == mine);
    std::atomic<DWORD> theirs { 0 };
    std::thread t([&] { theirs = GetCurrentThreadId(); });
    t.join();
    REQUIRE(theirs.load() != 0);
    REQUIRE(theirs.load() != mine);
}

TEST_CASE("sync: pseudo handles and DuplicateHandle of the current thread", "[sync]")
{
    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();
    CHECK(process == reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1)));
    CHECK(thread == reinterpret_cast<HANDLE>(static_cast<intptr_t>(-2)));
    CHECK(GetCurrentProcessId() != 0);

    HANDLE copy = NULL;
    REQUIRE(DuplicateHandle(process, thread, process, &copy, 0, TRUE, DUPLICATE_SAME_ACCESS) == TRUE);
    REQUIRE(copy != NULL);
    CHECK(copy != thread);
    CHECK(WaitForSingleObject(copy, 0) == WAIT_TIMEOUT);
    CHECK(GetThreadPriority(copy) == THREAD_PRIORITY_NORMAL);
    CHECK(SetThreadPriority(copy, 15) == TRUE);
    CHECK(GetThreadPriority(copy) == THREAD_PRIORITY_NORMAL);
    CHECK(CloseHandle(copy) == TRUE);

    HANDLE other = NULL;
    HANDLE event = CreateEventA(NULL, TRUE, FALSE, NULL);
    CHECK(DuplicateHandle(process, event, process, &other, 0, FALSE, DUPLICATE_SAME_ACCESS) == FALSE);
    CloseHandle(event);
    CHECK(GetThreadPriority(NULL) == THREAD_PRIORITY_ERROR_RETURN);
    CHECK(SetThreadPriority(NULL, 0) == FALSE);
    SetLastError(0);
}

namespace {

struct SuspendProbe {
    std::atomic<HANDLE> self { nullptr };
    std::atomic<bool> parked { false };
    std::atomic<bool> resumed { false };
    std::atomic<DWORD> previous { 99 };
};

void suspendingBody(void* arg)
{
    SuspendProbe* probe = static_cast<SuspendProbe*>(arg);
    while (probe->self.load() == nullptr)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    probe->parked = true;
    probe->previous = SuspendThread(probe->self.load());
    probe->resumed = true;
}

}

TEST_CASE("sync: a thread that suspends itself waits for ResumeThread", "[sync]")
{
    SuspendProbe probe;
    uintptr_t r = _beginthread(suspendingBody, 0, &probe);
    REQUIRE(r != static_cast<uintptr_t>(-1));
    HANDLE h = reinterpret_cast<HANDLE>(r);
    probe.self = h;
    REQUIRE(waitForFlag(probe.parked));
    // Wait until the thread is actually counted as suspended.
    DWORD count = 0;
    for (int i = 0; i < 500 && count == 0; ++i) {
        count = SuspendThread(h); // another thread: reports, never blocks
        if (count == 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    REQUIRE(count == 1);
    CHECK_FALSE(probe.resumed.load());
    CHECK(ResumeThread(h) == 1);
    REQUIRE(waitForFlag(probe.resumed));
    CHECK(probe.previous.load() == 0);
    CHECK(ResumeThread(h) == 0);
    REQUIRE(WaitForSingleObject(h, INFINITE) == WAIT_OBJECT_0);
    CHECK(CloseHandle(h) == TRUE);
    CHECK(SuspendThread(NULL) == static_cast<DWORD>(-1));
    CHECK(ResumeThread(NULL) == static_cast<DWORD>(-1));
}

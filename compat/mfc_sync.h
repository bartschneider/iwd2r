// iwd2r. Distributed under LICENSE.md.
// MFC synchronization classes built on the standard library. Depends only on
// win_types.h.
#pragma once

#include "win_types.h"

#include <chrono>
#include <condition_variable>
#include <mutex>

class CSyncObject {
public:
    virtual ~CSyncObject() = default;
    virtual BOOL Lock(DWORD timeout = INFINITE) = 0;
    virtual BOOL Unlock() = 0;
};

class CCriticalSection : public CSyncObject {
public:
    CCriticalSection() = default;
    CCriticalSection(const CCriticalSection&) = delete;
    CCriticalSection& operator=(const CCriticalSection&) = delete;

    BOOL Lock() { return Lock(INFINITE); }
    // The timeout is ignored: a critical section always waits.
    BOOL Lock(DWORD) override
    {
        m_mutex.lock();
        return TRUE;
    }
    BOOL Unlock() override
    {
        m_mutex.unlock();
        return TRUE;
    }

private:
    std::recursive_mutex m_mutex;
};

class CMutex : public CSyncObject {
public:
    explicit CMutex(BOOL initiallyOwn = FALSE)
    {
        if (initiallyOwn)
            m_mutex.lock();
    }
    CMutex(const CMutex&) = delete;
    CMutex& operator=(const CMutex&) = delete;

    BOOL Lock() { return Lock(INFINITE); }
    BOOL Lock(DWORD timeout) override
    {
        if (timeout == INFINITE) {
            m_mutex.lock();
            return TRUE;
        }
        return m_mutex.try_lock_for(std::chrono::milliseconds(timeout)) ? TRUE : FALSE;
    }
    BOOL Unlock() override
    {
        m_mutex.unlock();
        return TRUE;
    }

private:
    std::recursive_timed_mutex m_mutex;
};

class CEvent : public CSyncObject {
public:
    explicit CEvent(BOOL initiallyOwn = FALSE, BOOL manualReset = FALSE)
        : m_signaled(initiallyOwn != FALSE)
        , m_manual(manualReset != FALSE)
    {
    }
    CEvent(const CEvent&) = delete;
    CEvent& operator=(const CEvent&) = delete;

    BOOL Lock() { return Lock(INFINITE); }
    // Waits for the signaled state; an auto-reset event is reset on success.
    BOOL Lock(DWORD timeout) override
    {
        std::unique_lock<std::mutex> guard(m_lock);
        unsigned long long pulse = m_pulse;
        auto ready = [&] { return m_signaled || m_pulse != pulse; };
        ++m_waiters;
        bool ok = true;
        if (timeout == INFINITE)
            m_cv.wait(guard, ready);
        else
            ok = m_cv.wait_for(guard, std::chrono::milliseconds(timeout), ready);
        --m_waiters;
        if (ok && !m_manual)
            m_signaled = false;
        return ok ? TRUE : FALSE;
    }
    BOOL Unlock() override { return TRUE; }

    BOOL SetEvent()
    {
        {
            std::lock_guard<std::mutex> guard(m_lock);
            m_signaled = true;
        }
        if (m_manual)
            m_cv.notify_all();
        else
            m_cv.notify_one();
        return TRUE;
    }
    BOOL ResetEvent()
    {
        std::lock_guard<std::mutex> guard(m_lock);
        m_signaled = false;
        return TRUE;
    }
    // Releases the current waiters (all for manual-reset, one for auto-reset)
    // and leaves the event non-signaled.
    BOOL PulseEvent()
    {
        std::lock_guard<std::mutex> guard(m_lock);
        if (m_waiters > 0) {
            ++m_pulse;
            if (m_manual)
                m_cv.notify_all();
            else
                m_cv.notify_one();
        }
        m_signaled = false;
        return TRUE;
    }

private:
    std::mutex m_lock;
    std::condition_variable m_cv;
    bool m_signaled;
    bool m_manual;
    int m_waiters = 0;
    unsigned long long m_pulse = 0;
};

class CSingleLock {
public:
    explicit CSingleLock(CSyncObject* object, BOOL initialLock = FALSE)
        : m_object(object)
    {
        if (initialLock)
            Lock();
    }
    ~CSingleLock()
    {
        if (m_locked)
            Unlock();
    }
    CSingleLock(const CSingleLock&) = delete;
    CSingleLock& operator=(const CSingleLock&) = delete;

    BOOL Lock(DWORD timeout = INFINITE)
    {
        if (m_locked)
            return TRUE;
        m_locked = m_object->Lock(timeout) != FALSE;
        return m_locked ? TRUE : FALSE;
    }
    BOOL Unlock()
    {
        if (m_locked) {
            m_object->Unlock();
            m_locked = false;
        }
        return TRUE;
    }
    BOOL IsLocked() { return m_locked ? TRUE : FALSE; }

private:
    CSyncObject* m_object;
    bool m_locked = false;
};

// iwd2r. Distributed under LICENSE.md.
// Declarations of every Win32-named function the engine calls. All have C++
// linkage on purpose so mangled names never collide with the MSVC CRT or
// kernel32 symbols. One section per implementing file.
#pragma once

#include "compat/win_types.h"

#include <cstdint>

// ---- threads.cpp: threads, events, semaphores, mutexes, critical sections ----

struct CRITICAL_SECTION {
    void* impl;
};

uintptr_t _beginthread(void (*start)(void*), unsigned stackSize, void* arg);
DWORD GetCurrentThreadId();

DWORD WaitForSingleObject(HANDLE handle, DWORD milliseconds);
BOOL CloseHandle(HANDLE handle);

HANDLE CreateEventA(LPVOID attributes, BOOL manualReset, BOOL initialState, LPCSTR name);
BOOL SetEvent(HANDLE event);
BOOL ResetEvent(HANDLE event);

HANDLE CreateSemaphoreA(LPVOID attributes, LONG initial, LONG maximum, LPCSTR name);
BOOL ReleaseSemaphore(HANDLE semaphore, LONG count, LONG* previous);

HANDLE CreateMutexA(LPVOID attributes, BOOL initialOwner, LPCSTR name);
BOOL ReleaseMutex(HANDLE mutex);

void InitializeCriticalSection(CRITICAL_SECTION* section);
void EnterCriticalSection(CRITICAL_SECTION* section);
void LeaveCriticalSection(CRITICAL_SECTION* section);
void DeleteCriticalSection(CRITICAL_SECTION* section);
BOOL TryEnterCriticalSection(CRITICAL_SECTION* section);

// ---- clock.cpp: tick count, sleep and multimedia timers ----

#define TIME_ONESHOT 0
#define TIME_PERIODIC 1
#define TIMERR_NOERROR 0
#define MMSYSERR_INVALPARAM 11

typedef void(CALLBACK* LPTIMECALLBACK)(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);

DWORD GetTickCount();
DWORD timeGetTime();
void Sleep(DWORD milliseconds);
DWORD SleepEx(DWORD milliseconds, BOOL alertable);

MMRESULT timeSetEvent(UINT delayMs, UINT resolution, LPTIMECALLBACK callback, DWORD_PTR user, UINT flags);
MMRESULT timeKillEvent(UINT id);
MMRESULT timeBeginPeriod(UINT period);
MMRESULT timeEndPeriod(UINT period);

// Internal: the native half of SetProcessAffinityMask (threads.cpp). Kept in
// its own file because on Windows it includes <windows.h>, whose
// declarations clash with the shim's Win32 names; src/ never sees it.
#pragma once

#include <cstdint>

namespace platform {

// Restricts every thread of this process, and the threads it starts later,
// to the CPUs whose bits are set in mask (bit n is CPU n). False when the
// mask selects no usable CPU.
bool SetNativeProcessAffinity(uint64_t mask);

}

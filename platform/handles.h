// iwd2r. Distributed under LICENSE.md.
// Internal to the platform layer: kernel handles shared between threads.cpp,
// which owns CloseHandle, and fs.cpp, which opens files.
#pragma once

#include "compat/win_types.h"

#include <cstdio>

namespace platform::handles {

// A handle that owns file; CloseHandle closes it.
HANDLE MakeFileHandle(std::FILE* file);

// The file behind a MakeFileHandle handle, or nullptr for any other handle.
std::FILE* FileOf(HANDLE handle);

}

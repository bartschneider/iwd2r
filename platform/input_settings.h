// The system input settings the engine reads, as fixed values. Our window
// shim (platform/window.cpp) returns them instead of the machine's, and the
// replay recorder (oracle/replay/recorder.cpp) makes the original iwd2.exe
// see the same ones, so a double click or a key repeat lasts the same
// number of frames on both sides and on every machine. CChitin's
// InitializeVariables reads them (src/CChitin.cpp, GetDoubleClickTime,
// GetSystemMetrics, SystemParametersInfo).
#pragma once

namespace platform::input_settings {

// GetDoubleClickTime, in ms.
constexpr unsigned kDoubleClickTime = 500;
// GetSystemMetrics(SM_CXDOUBLECLK) and (SM_CYDOUBLECLK), in pixels.
constexpr int kDoubleClickWidth = 4;
constexpr int kDoubleClickHeight = 4;
// GetSystemMetrics(SM_SWAPBUTTON): buttons not swapped.
constexpr int kSwapButton = 0;
// SystemParametersInfo(SPI_GETKEYBOARDDELAY): 0 (shortest) to 3.
constexpr int kKeyboardDelay = 1;
// SystemParametersInfo(SPI_GETKEYBOARDSPEED): 0 (slowest) to 31.
constexpr int kKeyboardSpeed = 31;
// SystemParametersInfo(SPI_GETWHEELSCROLLLINES).
constexpr unsigned kWheelScrollLines = 3;

}

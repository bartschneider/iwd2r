// Internal: runs the GPU probe (`iwd2r --probe-gpu`) in a child process, so
// a Vulkan stack that damages the process it is loaded into (Windows
// Sandbox's) takes down the child, not the game. Used by the presenter
// front (present.cpp) before it creates a GPU device in the game's process.
#pragma once

#include <string>
#include <vector>

namespace platform::gpu_probe {

enum class Outcome {
    Ok, // exited 0 and printed "probe-gpu: ok ..."
    Failed, // exited non-zero (or 0 without the ok line), or could not be started
    Crashed, // killed by a signal (Linux) or ended by an exception (Windows)
    TimedOut, // still running at the deadline; killed
};

struct Result {
    Outcome outcome = Outcome::Failed;
    // The first stdout line starting with "probe-gpu:", without its end of
    // line; empty when the child printed none.
    std::string line;
    // A short reason for the log: the line without "probe-gpu: " (and
    // without "unavailable "), or
    // "crashed", "timed out", "exited with code N", "could not start: ...".
    std::string reason;
};

// Runs exe with args, stdout captured, stderr and the environment inherited,
// for at most timeoutMs milliseconds.
Result Run(const std::string& exe, int timeoutMs, const std::vector<std::string>& args = { "--probe-gpu" });

// The running executable's path (/proc/self/exe, GetModuleFileNameA);
// empty when it cannot be found.
std::string SelfExe();

}

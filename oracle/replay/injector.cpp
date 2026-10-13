// injector: starts the original game with the replay recorder loaded into it.
//
//   injector.exe <exe> <workdir> <dll> [--timeout S] [-- <game arguments>]
//
// Starts <exe> suspended, with <workdir> as its working directory, inside a
// kill-on-close job object; loads <dll> into it with CreateRemoteThread on
// LoadLibraryA; resumes it and waits. The game inherits this process's
// environment, so IWD2R_SCRIPT, IWD2R_REPLAY_DIR and IWD2R_DUMP_EVERY reach
// the recorder through CreateProcess's environment block.
//
// --timeout S (default none) kills the job after S seconds of wall-clock
// time. The recorder keeps the last frame it reached in
// <IWD2R_REPLAY_DIR>/status.txt; tools/oracle.py reports it.
//
// Exit codes: the game's own exit code when it ends by itself; 1 usage or a
// failure to start the game; 10 --timeout ran out and the game was killed;
// 11 the game died of an exception (the status is printed; WSL keeps only
// the low byte of an exit code); 12 the DLL could not be loaded (the remote
// LoadLibraryA returned NULL: the recorder refused to start, see its
// recorder.log, or the DLL is missing).
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr int kExitUsage = 1;
constexpr int kExitTimeout = 10;
constexpr int kExitChildDied = 11;
constexpr int kExitInjectFailed = 12;
constexpr DWORD kInjectWaitMs = 60000;

const char* StatusName(DWORD code)
{
    switch (code) {
    case 0xC0000005u:
        return "access violation";
    case 0xC00000FDu:
        return "stack overflow";
    case 0xC0000374u:
        return "heap corruption";
    case 0xC0000409u:
        return "fail-fast or stack buffer overrun";
    case 0xE06D7363u:
        return "uncaught C++ exception";
    default:
        return "unknown";
    }
}

// One argument quoted for a Windows command line (CommandLineToArgvW rules).
std::string Quote(const std::string& arg)
{
    if (!arg.empty() && arg.find_first_of(" \t\"") == std::string::npos) {
        return arg;
    }
    std::string out = "\"";
    size_t backslashes = 0;
    for (char c : arg) {
        if (c == '\\') {
            backslashes++;
            continue;
        }
        if (c == '"') {
            out.append(backslashes * 2 + 1, '\\');
        } else {
            out.append(backslashes, '\\');
        }
        backslashes = 0;
        out += c;
    }
    out.append(backslashes * 2, '\\');
    return out + "\"";
}

int Fail(const char* what, PROCESS_INFORMATION* child, HANDLE job)
{
    const DWORD error = GetLastError();
    std::fprintf(stderr, "injector: %s: error %lu\n", what, error);
    std::fflush(stderr);
    if (job) {
        TerminateJobObject(job, kExitUsage);
    } else if (child && child->hProcess) {
        TerminateProcess(child->hProcess, kExitUsage);
    }
    return kExitUsage;
}

// Loads dll into the suspended child with a remote LoadLibraryA. kernel32 sits
// at the same address in every process of one boot, so this process's
// LoadLibraryA is the child's. Returns the remote thread's exit code (the low
// 32 bits of the module handle; 0 when the load failed), or -1 when the
// remote thread could not run.
int64_t RemoteLoad(HANDLE process, const std::string& dll, DWORD* error)
{
    const SIZE_T size = dll.size() + 1;
    void* remote = VirtualAllocEx(process, nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!remote || !WriteProcessMemory(process, remote, dll.c_str(), size, nullptr)) {
        *error = GetLastError();
        return -1;
    }
    auto* load = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA")));
    HANDLE thread = CreateRemoteThread(process, nullptr, 0, load, remote, 0, nullptr);
    if (!thread) {
        *error = GetLastError();
        return -1;
    }
    if (WaitForSingleObject(thread, kInjectWaitMs) != WAIT_OBJECT_0) {
        *error = WAIT_TIMEOUT;
        CloseHandle(thread);
        return -1;
    }
    DWORD code = 0;
    GetExitCodeThread(thread, &code);
    CloseHandle(thread);
    VirtualFreeEx(process, remote, 0, MEM_RELEASE);
    return code;
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::string> positional;
    std::vector<std::string> gameArgs;
    unsigned timeoutSeconds = 0;
    bool usage = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--") == 0) {
            for (++i; i < argc; ++i) {
                gameArgs.push_back(argv[i]);
            }
            break;
        }
        if (std::strcmp(argv[i], "--timeout") == 0 && i + 1 < argc) {
            char* end = nullptr;
            timeoutSeconds = static_cast<unsigned>(std::strtoul(argv[++i], &end, 10));
            usage = usage || *end != '\0' || timeoutSeconds == 0;
        } else if (argv[i][0] == '-') {
            usage = true;
        } else {
            positional.push_back(argv[i]);
        }
    }
    if (usage || positional.size() != 3) {
        std::fprintf(stderr, "usage: injector <exe> <workdir> <dll> [--timeout S] [-- <game arguments>]\n");
        return kExitUsage;
    }
    char exe[MAX_PATH];
    char dll[MAX_PATH];
    if (!GetFullPathNameA(positional[0].c_str(), MAX_PATH, exe, nullptr)
        || !GetFullPathNameA(positional[2].c_str(), MAX_PATH, dll, nullptr)) {
        return Fail("cannot resolve the exe or DLL path", nullptr, nullptr);
    }
    if (GetFileAttributesA(dll) == INVALID_FILE_ATTRIBUTES) {
        std::fprintf(stderr, "injector: no DLL %s\n", dll);
        return kExitInjectFailed;
    }
    std::string command = Quote(exe);
    for (const std::string& arg : gameArgs) {
        command += " " + Quote(arg);
    }

    // The game dies with this process (Ctrl+C, a killed run): it is created
    // already inside the job, as RunChild in oracle/runner/oracle_main.cpp
    // does, so there is no moment in which a suspended game can be left
    // behind.
    HANDLE job = CreateJobObjectA(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!job || !SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
        return Fail("cannot create the kill-on-close job", nullptr, nullptr);
    }
    SIZE_T attributeSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeSize);
    std::vector<uint8_t> attributeBuffer(attributeSize);
    STARTUPINFOEXA startupEx = {};
    startupEx.StartupInfo.cb = sizeof(startupEx);
    startupEx.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributeBuffer.data());
    if (!InitializeProcThreadAttributeList(startupEx.lpAttributeList, 1, 0, &attributeSize)
        || !UpdateProcThreadAttribute(startupEx.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_JOB_LIST, &job, sizeof(job),
            nullptr, nullptr)) {
        return Fail("cannot put the game in its job", nullptr, nullptr);
    }
    PROCESS_INFORMATION child = {};
    // nullptr environment: the game gets this process's, IWD2R_* included.
    const BOOL created = CreateProcessA(exe, command.data(), nullptr, nullptr, FALSE,
        CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT, nullptr, positional[1].c_str(), &startupEx.StartupInfo,
        &child);
    DeleteProcThreadAttributeList(startupEx.lpAttributeList);
    if (!created) {
        return Fail("cannot start the game suspended in its job", nullptr, nullptr);
    }
    DWORD error = 0;
    const int64_t loaded = RemoteLoad(child.hProcess, dll, &error);
    if (loaded <= 0) {
        TerminateJobObject(job, kExitInjectFailed);
        WaitForSingleObject(child.hProcess, 10000);
        if (loaded < 0) {
            std::fprintf(stderr, "injector: cannot run LoadLibraryA in the game: error %lu\n", error);
        } else {
            std::fprintf(stderr, "injector: LoadLibraryA(%s) failed in the game; see recorder.log in the replay "
                                 "folder\n",
                dll);
        }
        std::fflush(stderr);
        return kExitInjectFailed;
    }
    std::printf("injector: %s loaded into %s (pid %lu)\n", dll, exe, child.dwProcessId);
    std::fflush(stdout);
    ResumeThread(child.hThread);
    const DWORD waited
        = WaitForSingleObject(child.hProcess, timeoutSeconds ? static_cast<DWORD>(timeoutSeconds) * 1000 : INFINITE);
    if (waited != WAIT_OBJECT_0) {
        TerminateJobObject(job, kExitTimeout);
        WaitForSingleObject(child.hProcess, 10000);
        std::fprintf(stderr, "injector: timed out after %u s; the game was killed\n", timeoutSeconds);
        std::fflush(stderr);
        return kExitTimeout;
    }
    DWORD code = 1;
    GetExitCodeProcess(child.hProcess, &code);
    CloseHandle(child.hThread);
    CloseHandle(child.hProcess);
    if (code >= 0xC0000000u || code == 0xE06D7363u) {
        std::fprintf(stderr, "injector: the game died with exception 0x%08lX (%s)\n", code, StatusName(code));
        std::fflush(stderr);
        return kExitChildDied;
    }
    std::printf("injector: the game exited with code %lu\n", code);
    std::fflush(stdout);
    return static_cast<int>(code);
}

#include "platform/gpu_probe.h"

#include "platform/crash.h"

#include <chrono>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <climits>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>

#include <cstdint>
#include <cstdlib>
#include <vector>
#endif

extern char** environ;
#endif

namespace platform::gpu_probe {

namespace {

    const char kPrefix[] = "probe-gpu: ";

    // The probe prints one line: keep the first 4 KB of stdout, and drain
    // (and drop) the rest so a chatty child never blocks on a full pipe.
    const size_t kMaxOutput = 4096;
    // Reads per drain: a child (or a grandchild holding the pipe) that never
    // stops writing cannot hold the parent in one drain.
    const int kMaxReadsPerDrain = 64;
    // How often the parent looks at the child while waiting.
    const int kSliceMs = 20;

    // The child's environment: this process's, with IWD2R_CRASH_PREFIX set
    // to probe-gpu so a crash in the child reads "probe-gpu crash: ..."
    // before the parent's fallback line.
    std::vector<std::string> ChildEnvironment(const std::vector<std::string>& own)
    {
        const std::string name = std::string(crash::kPrefixVariable) + "=";
        std::vector<std::string> env;
        for (const std::string& entry : own) {
            if (entry.compare(0, name.size(), name) != 0)
                env.push_back(entry);
        }
        env.push_back(name + "probe-gpu");
        return env;
    }

    void Keep(std::string& out, const char* data, size_t n)
    {
        if (out.size() < kMaxOutput)
            out.append(data, n < kMaxOutput - out.size() ? n : kMaxOutput - out.size());
    }

    // Fills line and reason from what the child printed and how it ended.
    void Finish(Result& r, const std::string& out, Outcome ended, const std::string& endedReason)
    {
        size_t start = 0;
        while (start < out.size()) {
            size_t end = out.find('\n', start);
            if (end == std::string::npos)
                end = out.size();
            std::string line = out.substr(start, end - start);
            while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
                line.pop_back();
            if (line.compare(0, sizeof(kPrefix) - 2, kPrefix, sizeof(kPrefix) - 2) == 0) {
                r.line = line;
                break;
            }
            start = end + 1;
        }
        std::string said = r.line.size() >= sizeof(kPrefix) - 1 ? r.line.substr(sizeof(kPrefix) - 1) : std::string();
        // "unavailable <reason>": the reason is enough.
        const char unavailable[] = "unavailable ";
        if (said.compare(0, sizeof(unavailable) - 1, unavailable) == 0)
            said.erase(0, sizeof(unavailable) - 1);
        r.outcome = ended;
        if (ended == Outcome::Ok && said.compare(0, 2, "ok") != 0)
            r.outcome = Outcome::Failed;
        switch (r.outcome) {
        case Outcome::Ok:
            r.reason = said;
            break;
        case Outcome::Failed:
            r.reason = !said.empty() ? said : !endedReason.empty() ? endedReason
                                                                   : std::string("printed no result");
            break;
        case Outcome::Crashed:
            r.reason = said.empty() ? std::string("crashed") : "crashed after \"" + said + "\"";
            break;
        case Outcome::TimedOut:
            r.reason = "timed out";
            break;
        }
    }

#ifdef _WIN32

    // CommandLineToArgvW's quoting rules.
    std::string Quote(const std::string& arg)
    {
        if (!arg.empty() && arg.find_first_of(" \t\"") == std::string::npos)
            return arg;
        std::string q = "\"";
        size_t slashes = 0;
        for (char c : arg) {
            if (c == '\\') {
                slashes++;
                continue;
            }
            q.append(c == '"' ? slashes * 2 + 1 : slashes, '\\');
            slashes = 0;
            q += c;
        }
        q.append(slashes * 2, '\\');
        q += '"';
        return q;
    }

#endif

}

#ifdef _WIN32

Result Run(const std::string& exe, int timeoutMs, const std::vector<std::string>& args)
{
    Result r;
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE readEnd = nullptr;
    HANDLE writeEnd = nullptr;
    if (!CreatePipe(&readEnd, &writeEnd, &sa, 0)) {
        Finish(r, std::string(), Outcome::Failed, "could not start: no pipe");
        return r;
    }
    SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0);

    std::string cmd = Quote(exe);
    for (const std::string& a : args)
        cmd += " " + Quote(a);
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = nullptr;
    si.hStdOutput = writeEnd;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION pi = {};
    // SEM_NOGPFAULTERRORBOX: a crashing child exits without a WER dialog.
    const UINT oldMode = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    // The environment block: NAME=value strings, each ending in a NUL, then
    // one more NUL.
    std::vector<std::string> own;
    if (char* block = GetEnvironmentStringsA()) {
        for (const char* p = block; *p != '\0'; p += std::strlen(p) + 1)
            own.push_back(p);
        FreeEnvironmentStringsA(block);
    }
    std::string envBlock;
    for (const std::string& entry : ChildEnvironment(own))
        envBlock += entry + '\0';
    envBlock += '\0';
    const BOOL started = CreateProcessA(exe.c_str(), cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, envBlock.data(), nullptr, &si, &pi);
    SetErrorMode(oldMode);
    CloseHandle(writeEnd);
    if (!started) {
        CloseHandle(readEnd);
        Finish(r, std::string(), Outcome::Failed, "could not start: error " + std::to_string(GetLastError()));
        return r;
    }
    CloseHandle(pi.hThread);

    std::string out;
    auto drain = [&]() {
        DWORD avail = 0;
        for (int reads = 0; reads < kMaxReadsPerDrain && PeekNamedPipe(readEnd, nullptr, 0, nullptr, &avail, nullptr) && avail > 0; reads++) {
            char buf[512];
            DWORD got = 0;
            if (!ReadFile(readEnd, buf, avail < sizeof(buf) ? avail : static_cast<DWORD>(sizeof(buf)), &got, nullptr) || got == 0)
                break;
            Keep(out, buf, got);
        }
    };
    // Drain while waiting, so a chatty child never blocks on a full pipe.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    DWORD wait = WAIT_TIMEOUT;
    for (;;) {
        drain();
        wait = WaitForSingleObject(pi.hProcess, kSliceMs);
        if (wait != WAIT_TIMEOUT || std::chrono::steady_clock::now() >= deadline)
            break;
    }
    drain();
    Outcome ended = Outcome::Failed;
    std::string why;
    if (wait == WAIT_OBJECT_0) {
        DWORD code = 1;
        GetExitCodeProcess(pi.hProcess, &code);
        // NTSTATUS error codes (0xC0000005 and the like): an exception.
        if (code >= 0xC0000000u) {
            ended = Outcome::Crashed;
        } else if (code == 0) {
            ended = Outcome::Ok;
        } else {
            why = "exited with code " + std::to_string(code);
        }
    } else {
        TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, 5000);
        ended = Outcome::TimedOut;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(readEnd);
    Finish(r, out, ended, why);
    return r;
}

std::string SelfExe()
{
    char buffer[MAX_PATH * 4];
    const DWORD n = GetModuleFileNameA(nullptr, buffer, sizeof(buffer));
    if (n == 0 || n >= sizeof(buffer))
        return std::string();
    return std::string(buffer, n);
}

#else

Result Run(const std::string& exe, int timeoutMs, const std::vector<std::string>& args)
{
    Result r;
    int fds[2];
    if (pipe2(fds, O_CLOEXEC) != 0) {
        Finish(r, std::string(), Outcome::Failed, std::string("could not start: ") + std::strerror(errno));
        return r;
    }
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(exe.c_str()));
    for (const std::string& a : args)
        argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    // dup2 clears close-on-exec on the child's stdout; both pipe ends close.
    posix_spawn_file_actions_adddup2(&actions, fds[1], STDOUT_FILENO);
    std::vector<std::string> own;
    for (char** e = environ; *e != nullptr; e++)
        own.push_back(*e);
    const std::vector<std::string> env = ChildEnvironment(own);
    std::vector<char*> envp;
    for (const std::string& entry : env)
        envp.push_back(const_cast<char*>(entry.c_str()));
    envp.push_back(nullptr);
    pid_t pid = -1;
    const int err = posix_spawn(&pid, exe.c_str(), &actions, nullptr, argv.data(), envp.data());
    posix_spawn_file_actions_destroy(&actions);
    close(fds[1]);
    if (err != 0) {
        close(fds[0]);
        Finish(r, std::string(), Outcome::Failed, std::string("could not start: ") + std::strerror(err));
        return r;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    auto remaining = [&]() {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
        return left > 0 ? static_cast<int>(left < INT_MAX ? left : INT_MAX) : 0;
    };
    std::string out;
    bool open = true; // the read end has not reached EOF
    // Reads what is in the pipe now, waiting at most waitMs for the first byte.
    auto drain = [&](int waitMs) {
        for (int reads = 0; open && reads < kMaxReadsPerDrain; reads++) {
            pollfd p = { fds[0], POLLIN, 0 };
            const int n = poll(&p, 1, reads == 0 ? waitMs : 0);
            if (n < 0 && errno == EINTR)
                continue;
            if (n <= 0)
                return;
            char buf[512];
            const ssize_t got = read(fds[0], buf, sizeof(buf));
            if (got < 0 && errno == EINTR)
                continue;
            if (got <= 0) {
                open = false; // EOF (or an error): the exit decides the rest
                return;
            }
            Keep(out, buf, static_cast<size_t>(got));
        }
    };

    // The child's exit ends the wait, not EOF: a grandchild that inherited
    // stdout would hold the pipe open. Drain in short slices meanwhile.
    int status = 0;
    pid_t done = 0;
    for (;;) {
        done = waitpid(pid, &status, WNOHANG);
        if (done < 0 && errno == EINTR)
            continue;
        if (done != 0 || remaining() <= 0)
            break;
        const int slice = remaining() < kSliceMs ? remaining() : kSliceMs;
        if (open)
            drain(slice);
        else
            poll(nullptr, 0, slice);
    }
    if (done == 0) {
        // The deadline: kill before the read end closes, so the child cannot
        // die of SIGPIPE in between and read as a crash.
        kill(pid, SIGKILL);
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
        }
    } else {
        drain(0); // what the child wrote before it exited
    }
    close(fds[0]);

    Outcome ended = Outcome::Failed;
    std::string why;
    if (done == pid) {
        if (WIFSIGNALED(status)) {
            ended = Outcome::Crashed;
        } else if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            ended = Outcome::Ok;
        } else {
            why = "exited with code " + std::to_string(WIFEXITED(status) ? WEXITSTATUS(status) : -1);
        }
    } else if (done == 0) {
        ended = Outcome::TimedOut;
    } else {
        why = std::string("lost the child: ") + std::strerror(errno);
    }
    Finish(r, out, ended, why);
    return r;
}

std::string SelfExe()
{
#ifdef __APPLE__
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> path(size + 1, '\0');
    if (size == 0 || _NSGetExecutablePath(path.data(), &size) != 0)
        return std::string();
    char resolved[PATH_MAX];
    if (!realpath(path.data(), resolved))
        return std::string(path.data());
    return std::string(resolved);
#else
    char buffer[4096];
    const ssize_t n = readlink("/proc/self/exe", buffer, sizeof(buffer));
    if (n <= 0 || static_cast<size_t>(n) >= sizeof(buffer))
        return std::string();
    return std::string(buffer, static_cast<size_t>(n));
#endif
}

#endif

}

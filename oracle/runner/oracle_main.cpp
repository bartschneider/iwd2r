// iwd2r. Distributed under LICENSE.md.
// iwd2r_oracle: maps the vanilla iwd2.exe at 0x400000 and enters it through its
// own CRT, whose call to WinMain is redirected to OracleMain.
//
//   iwd2r_oracle.exe [<iwd2.exe>] --file <name> [--seed N] [--iterations N] [--out <dir>]
//   iwd2r_oracle.exe [<iwd2.exe>] --selftest [--test-bad-callsite] [--test-bad-target] [--test-occupy]
//   iwd2r_oracle.exe --list
//
// The exe defaults to iwd2.exe in the current folder.
//
// --file runs the cases of oracle/cases/<name>.cpp: OracleMain seeds the
// original rand with the low 32 bits of --seed (default 1), snapshots .data
// and the thread's rand state, and before each case restores them. Each case
// prints one line; the report goes to <out>/<name>.json (default: the current
// folder). --iterations (default 10000) bounds unbounded generators. --list
// prints the case file names, one per line.
//
// --selftest prints machine-checkable lines that tools/oracle.py compares with
// the exe file: `mem <addr> <dword>` for a few image addresses, `import <addr>
// <dll>!<name> <address>` for those that are IAT slots, and `rand <value>` for
// the original rand() after the original srand(1).
// --test-bad-callsite patches one byte past the recorded WinMain call site and
// must be refused; --test-bad-target expects a wrong WinMain address at the
// right call site and must be refused too; --test-occupy takes 0x400000 before the reservation and
// must be reported with the occupant's name.
//
// The program runs twice: the parent starts a suspended copy with --child,
// reserves 0x400000-0xA21000 in it, then resumes it and passes on its exit
// code. The child maps the exe into that reservation. The child is created
// inside a kill-on-close job, so it never outlives the parent.
//
// Exit codes: 0 success (every case as declared), 1 usage or load error, 3 an
// unresolved import was called, 4 the image range is occupied, 5 wrong MD5,
// 6 call site refused, 7 a case did not do what it declared, 8 the snapshot
// was used off its thread.
#include "oracle/loader/pe.h"
#include "oracle/runner/registry.h"
#include "oracle/runner/report.h"
#include "vanilla_config.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <bcrypt.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kTestOccupyAddress = 0x400000;
constexpr const char* kChildFlag = "--child";

bool g_selftest = false;
PeImage g_image;
std::string g_file; // --file
uint64_t g_seed = 1;
int g_iterations = 10000;
std::string g_out = ".";

// Starts this program again, suspended, with --child; reserves the vanilla
// range in it before any of its code runs; resumes it and returns its exit
// code. The child cannot reserve the range itself: by the time its first TLS
// callback runs, Windows has mapped NLS tables at 0x400000.
int RunChild(bool occupy)
{
    std::string command = std::string(GetCommandLineA()) + " " + kChildFlag;
    STARTUPINFOA startup = {};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    HANDLE* handles[] = { &startup.hStdInput, &startup.hStdOutput, &startup.hStdError };
    const DWORD ids[] = { STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE };
    for (int i = 0; i < 3; ++i) {
        *handles[i] = GetStdHandle(ids[i]);
        if (*handles[i] && *handles[i] != INVALID_HANDLE_VALUE) {
            SetHandleInformation(*handles[i], HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        }
    }
    // The child dies with this process (Ctrl+C, a killed run). It is created
    // already inside the job, so there is no moment in which the parent can
    // die and leave a suspended child behind; without the job, no child.
    HANDLE job = CreateJobObjectA(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!job || !SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
        std::fprintf(stderr, "oracle: cannot create the kill-on-close job: error %lu\n", GetLastError());
        return 1;
    }
    SIZE_T attributeSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeSize);
    std::vector<uint8_t> attributeBuffer(attributeSize);
    STARTUPINFOEXA startupEx = {};
    startupEx.StartupInfo = startup;
    startupEx.StartupInfo.cb = sizeof(startupEx);
    startupEx.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributeBuffer.data());
    if (!InitializeProcThreadAttributeList(startupEx.lpAttributeList, 1, 0, &attributeSize)
        || !UpdateProcThreadAttribute(startupEx.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_JOB_LIST, &job, sizeof(job),
            nullptr, nullptr)) {
        std::fprintf(stderr, "oracle: cannot put the runner child in its job: error %lu\n", GetLastError());
        return 1;
    }
    PROCESS_INFORMATION child = {};
    const BOOL created = CreateProcessA(nullptr, command.data(), nullptr, nullptr, TRUE,
        CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr, &startupEx.StartupInfo, &child);
    const DWORD createError = GetLastError();
    DeleteProcThreadAttributeList(startupEx.lpAttributeList);
    if (!created) {
        std::fprintf(stderr, "oracle: cannot start the runner child in its job: error %lu\n", createError);
        return 1;
    }
    if (occupy) {
        VirtualAllocEx(child.hProcess, reinterpret_cast<void*>(static_cast<uintptr_t>(kTestOccupyAddress)), 0x1000,
            MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    }
    std::string occupant;
    if (!ReserveImageRange(child.hProcess, &occupant)) {
        TerminateProcess(child.hProcess, 4);
        std::fprintf(stderr, "oracle: cannot reserve 0x00400000-0x00A21000 for iwd2.exe: occupied by %s\n",
            occupant.c_str());
        return 4;
    }
    ResumeThread(child.hThread);
    WaitForSingleObject(child.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(child.hProcess, &code);
    CloseHandle(child.hThread);
    CloseHandle(child.hProcess);
    return static_cast<int>(code);
}

bool Md5Hex(const std::vector<uint8_t>& data, std::string* out)
{
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_MD5_ALGORITHM, nullptr, 0) != 0) {
        return false;
    }
    uint8_t digest[16];
    const bool ok = BCryptHash(algorithm, nullptr, 0, const_cast<PUCHAR>(data.data()),
                        static_cast<ULONG>(data.size()), digest, sizeof(digest))
        == 0;
    BCryptCloseAlgorithmProvider(algorithm, 0);
    out->clear();
    for (uint8_t byte : digest) {
        char hex[3];
        std::snprintf(hex, sizeof(hex), "%02x", byte);
        *out += hex;
    }
    return ok;
}

uint32_t ReadDword(uint32_t address)
{
    uint32_t value = 0;
    std::memcpy(&value, reinterpret_cast<const void*>(static_cast<uintptr_t>(address)), 4);
    return value;
}

// The import whose IAT slot is at address, or nullptr.
const PeImport* ImportAt(uint32_t address)
{
    for (const PeImport& import : g_image.imports) {
        if (g_image.imageBase + import.iatRva == address) {
            return &import;
        }
    }
    return nullptr;
}

void PrintMem(uint32_t address)
{
    std::printf("mem %08X %08x\n", address, ReadDword(address));
    if (const PeImport* import = ImportAt(address)) {
        HMODULE module = GetModuleHandleA(import->dll.c_str());
        FARPROC proc = module ? GetProcAddress(module,
                                    import->name.empty() ? MAKEINTRESOURCEA(import->ordinal) : import->name.c_str())
                              : nullptr;
        std::printf("import %08X %s!%s %08x\n", address, import->dll.c_str(),
            import->name.empty() ? ("#" + std::to_string(import->ordinal)).c_str() : import->name.c_str(),
            static_cast<uint32_t>(reinterpret_cast<uintptr_t>(proc)));
    }
}

int SelfTest()
{
    // .text start; .rdata start, which is the first IAT slot; the first
    // nonzero .rdata dword past the IAT; and .rsrc's entry counts, whose file
    // offset (0x4C600C) differs from the RVA.
    for (uint32_t address : { 0x401000u, 0x847000u, 0x8475B0u, 0xA1B00Cu }) {
        PrintMem(address);
    }
    using Srand = void(__cdecl*)(unsigned int);
    using Rand = int(__cdecl*)();
    reinterpret_cast<Srand>(static_cast<uintptr_t>(kVanillaSrand))(1);
    std::printf("rand %d\n", reinterpret_cast<Rand>(static_cast<uintptr_t>(kVanillaRand))());
    return 0;
}

uint32_t ParseUnsigned(const char* text, bool* ok)
{
    char* end = nullptr;
    const unsigned long long value = std::strtoull(text, &end, 0);
    *ok = end != text && *end == '\0';
    return static_cast<uint32_t>(value);
}

// The case file names, each once, in registration order.
std::vector<std::string> CaseFiles()
{
    std::vector<std::string> files;
    for (const OracleCase& c : OracleCases()) {
        const std::string name = CaseFileName(c);
        bool seen = false;
        for (const std::string& file : files) {
            seen = seen || file == name;
        }
        if (!seen) {
            files.push_back(name);
        }
    }
    return files;
}

const char* Outcome(const CaseResult& r)
{
    if (r.crashed) {
        return r.expect == OracleExpect::Crash ? "crashed (expected)" : "CRASHED";
    }
    if (r.failureCount > 0) {
        return r.expect == OracleExpect::Fail ? "failed (expected)" : "FAILED";
    }
    return r.expect == OracleExpect::Pass ? "passed" : "PASSED (expected to fail)";
}

// Runs the cases of g_file. Called from OracleMain on the original's main
// thread, after its CRT initialisation.
int RunCases()
{
    const PeSection* data = nullptr;
    for (const PeSection& section : g_image.sections) {
        if (section.name == ".data") {
            data = &section;
        }
    }
    if (!data) {
        std::fprintf(stderr, "oracle: the image has no .data section\n");
        return 1;
    }
    // Every case starts from srand(seed): seed the original before the
    // snapshot, which holds the thread's rand state.
    reinterpret_cast<void(__cdecl*)(unsigned int)>(static_cast<uintptr_t>(kVanillaSrand))(
        static_cast<unsigned int>(g_seed));
    const uint32_t dataStart = g_image.imageBase + data->rva;
    std::string error;
    if (!TakeSnapshot(DataRange { dataStart, dataStart + data->virtualSize }, &error)) {
        std::fprintf(stderr, "oracle: cannot snapshot .data: %s\n", error.c_str());
        return 1;
    }
    std::printf("oracle: %s, seed %llu, iterations %d\n", g_file.c_str(), static_cast<unsigned long long>(g_seed),
        g_iterations);
    std::vector<CaseResult> results;
    bool allExpected = true;
    for (const OracleCase& c : OracleCases()) {
        if (CaseFileName(c) != g_file) {
            continue;
        }
        RestoreData();
        CaseContext ctx(g_seed, g_iterations);
        try {
            c.run(ctx);
        } catch (const CaseCrashed&) {
            // Recorded in ctx; the next case starts from the snapshot.
        } catch (const std::exception& e) {
            ctx.Fail(ctx.Inputs(), "no C++ exception", std::string("exception: ") + e.what());
        }
        CaseResult r;
        r.id = c.id;
        r.expect = c.expect;
        r.iterations = ctx.InputsRun();
        r.failureCount = ctx.FailureCount();
        r.failures = ctx.Failures();
        r.crashed = ctx.Crashed();
        r.crash = ctx.Crash();
        allExpected = allExpected && AsExpected(r);
        std::printf("%-32s %s (%lld inputs, %lld failures)\n", r.id.c_str(), Outcome(r), r.iterations,
            r.failureCount);
        for (size_t i = 0; i < r.failures.size() && i < 3; ++i) {
            std::printf("    seed %llu, inputs %s: original %s, ours %s\n", static_cast<unsigned long long>(g_seed),
                r.failures[i].inputs.c_str(), r.failures[i].original.c_str(), r.failures[i].ours.c_str());
        }
        if (r.crashed) {
            std::printf("    seed %llu, inputs %s: exception 0x%08X at 0x%08X\n",
                static_cast<unsigned long long>(g_seed), r.crash.inputs.c_str(), r.crash.code, r.crash.address);
        }
        std::fflush(stdout);
        results.push_back(std::move(r));
    }
    const std::string path = g_out + "\\" + g_file + ".json";
    std::FILE* out = std::fopen(path.c_str(), "wb");
    const std::string json = ReportJson(g_file, g_seed, g_iterations, results);
    if (!out || std::fwrite(json.data(), 1, json.size(), out) != json.size() || std::fclose(out) != 0) {
        std::fprintf(stderr, "oracle: cannot write %s\n", path.c_str());
        return 1;
    }
    std::printf("oracle: wrote %s\n", path.c_str());
    return allExpected ? 0 : 7;
}

} // namespace

std::vector<OracleCase>& OracleCases()
{
    static std::vector<OracleCase> cases;
    return cases;
}

std::string CaseFileName(const OracleCase& c)
{
    std::string path = c.file;
    const size_t slash = path.find_last_of("/\\");
    if (slash != std::string::npos) {
        path = path.substr(slash + 1);
    }
    return path.substr(0, path.find('.'));
}

// Replaces WinMain: the original CRT has initialised its heap, stdio and static
// constructors and calls this on its main thread. Its exit() ends the process
// with the value returned here.
int __stdcall OracleMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    int code = 1;
    if (g_selftest) {
        code = SelfTest();
    } else {
        code = RunCases();
    }
    std::fflush(stdout);
    std::fflush(stderr);
    return code;
}

int main(int argc, char** argv)
{
    const char* exe = nullptr;
    bool badCallSite = false;
    bool badTarget = false;
    bool occupy = false;
    bool child = false;
    bool list = false;
    bool usage = false;
    for (int i = 1; i < argc && !usage; ++i) {
        const bool hasValue = i + 1 < argc;
        bool ok = true;
        if (std::strcmp(argv[i], "--selftest") == 0) {
            g_selftest = true;
        } else if (std::strcmp(argv[i], "--test-bad-callsite") == 0) {
            badCallSite = true;
        } else if (std::strcmp(argv[i], "--test-bad-target") == 0) {
            badTarget = true;
        } else if (std::strcmp(argv[i], "--test-occupy") == 0) {
            occupy = true;
        } else if (std::strcmp(argv[i], kChildFlag) == 0) {
            child = true;
        } else if (std::strcmp(argv[i], "--list") == 0) {
            list = true;
        } else if (std::strcmp(argv[i], "--file") == 0 && hasValue) {
            g_file = argv[++i];
        } else if (std::strcmp(argv[i], "--out") == 0 && hasValue) {
            g_out = argv[++i];
        } else if (std::strcmp(argv[i], "--seed") == 0 && hasValue) {
            char* end = nullptr;
            g_seed = std::strtoull(argv[++i], &end, 0);
            ok = end != argv[i] && *end == '\0';
        } else if (std::strcmp(argv[i], "--iterations") == 0 && hasValue) {
            g_iterations = static_cast<int>(ParseUnsigned(argv[++i], &ok));
            ok = ok && g_iterations > 0;
        } else if (!exe && argv[i][0] != '-') {
            exe = argv[i];
        } else {
            ok = false;
        }
        usage = !ok;
    }
    if (list && !usage) {
        for (const std::string& file : CaseFiles()) {
            std::printf("%s\n", file.c_str());
        }
        return 0;
    }
    if (!exe) {
        exe = "iwd2.exe";
    }
    if (!usage && !g_selftest) {
        bool known = false;
        for (const std::string& file : CaseFiles()) {
            known = known || file == g_file;
        }
        if (!known) {
            std::fprintf(stderr, "oracle: no case file '%s'; --list names them\n", g_file.c_str());
            return 1;
        }
    }
    if (usage || (g_selftest && !g_file.empty())) {
        std::fprintf(stderr,
            "usage: iwd2r_oracle [<iwd2.exe>] --file <name> [--seed N] [--iterations N] [--out <dir>]\n"
            "       iwd2r_oracle [<iwd2.exe>] --selftest [--test-bad-callsite] [--test-bad-target] [--test-occupy]\n"
            "       iwd2r_oracle --list\n");
        return 1;
    }
    if (!child) {
        return RunChild(occupy);
    }
    std::string error;
    if (!CheckImageRangeReserved(&error)) {
        std::fprintf(stderr, "oracle: %s\n", error.c_str());
        return 4;
    }
    std::ifstream in(exe, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "oracle: cannot read %s\n", exe);
        return 1;
    }
    const std::vector<uint8_t> file((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::string md5;
    if (!Md5Hex(file, &md5)) {
        std::fprintf(stderr, "oracle: cannot hash %s\n", exe);
        return 1;
    }
    if (md5 != kVanillaMd5) {
        std::fprintf(stderr, "oracle: %s has MD5 %s; the oracle needs the vanilla iwd2.exe (%s)\n", exe,
            md5.c_str(), kVanillaMd5);
        return 5;
    }
    if (!ParsePe(file, &g_image, &error) || !MapImage(g_image, file, &error)
        || !ResolveImports(g_image, &error)) {
        std::fprintf(stderr, "oracle: cannot load %s: %s\n", exe, error.c_str());
        return 1;
    }
    const uint32_t callSite = kVanillaWinMainCallSite + (badCallSite ? 1 : 0);
    const uint32_t winMain = kVanillaWinMain + (badTarget ? 0x10 : 0);
    if (!PatchWinMainCall(callSite, winMain, reinterpret_cast<void*>(&OracleMain), &error)) {
        std::fprintf(stderr, "oracle: refusing to patch the WinMain call: %s\n", error.c_str());
        return 6;
    }
    EnterImage(g_image);
}

// iwd2r. Distributed under LICENSE.md.
// iwd2r_oracle: maps the vanilla iwd2.exe at 0x400000 and enters it through its
// own CRT, whose call to WinMain is redirected to OracleMain.
//
//   iwd2r_oracle.exe <iwd2.exe> [--selftest] [--test-bad-callsite] [--test-occupy]
//
// --selftest prints machine-checkable lines that tools/oracle.py compares with
// the exe file: `mem <addr> <dword>` for a few image addresses, `import <addr>
// <dll>!<name> <address>` for those that are IAT slots, and `rand <value>` for
// the original rand() after the original srand(1).
// --test-bad-callsite patches one byte past the recorded WinMain call site and
// must be refused; --test-occupy takes 0x400000 before the reservation and
// must be reported with the occupant's name.
//
// The program runs twice: the parent starts a suspended copy with --child,
// reserves 0x400000-0xA21000 in it, then resumes it and passes on its exit
// code. The child maps the exe into that reservation.
//
// Exit codes: 0 success, 1 usage or load error, 3 an unresolved import was
// called, 4 the image range is occupied, 5 wrong MD5, 6 call site refused.
#include "oracle/loader/pe.h"
#include "vanilla_config.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <bcrypt.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

namespace {

constexpr uint32_t kTestOccupyAddress = 0x400000;
constexpr const char* kChildFlag = "--child";

bool g_selftest = false;
PeImage g_image;

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
    // The child dies with this process (Ctrl+C, a killed run).
    HANDLE job = CreateJobObjectA(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (job) {
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
    }
    PROCESS_INFORMATION child = {};
    if (!CreateProcessA(nullptr, command.data(), nullptr, nullptr, TRUE, CREATE_SUSPENDED, nullptr, nullptr, &startup,
            &child)) {
        std::fprintf(stderr, "oracle: cannot start the runner child: error %lu\n", GetLastError());
        return 1;
    }
    if (job) {
        AssignProcessToJobObject(job, child.hProcess);
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

} // namespace

// Replaces WinMain: the original CRT has initialised its heap, stdio and static
// constructors and calls this on its main thread. Its exit() ends the process
// with the value returned here.
int __stdcall OracleMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    int code = 1;
    if (g_selftest) {
        code = SelfTest();
    } else {
        std::fprintf(stderr, "oracle: no suite selected; pass --selftest\n");
    }
    std::fflush(stdout);
    std::fflush(stderr);
    return code;
}

int main(int argc, char** argv)
{
    const char* exe = nullptr;
    bool badCallSite = false;
    bool occupy = false;
    bool child = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--selftest") == 0) {
            g_selftest = true;
        } else if (std::strcmp(argv[i], "--test-bad-callsite") == 0) {
            badCallSite = true;
        } else if (std::strcmp(argv[i], "--test-occupy") == 0) {
            occupy = true;
        } else if (std::strcmp(argv[i], kChildFlag) == 0) {
            child = true;
        } else if (!exe && argv[i][0] != '-') {
            exe = argv[i];
        } else {
            exe = nullptr;
            break;
        }
    }
    if (!exe) {
        std::fprintf(stderr,
            "usage: iwd2r_oracle <iwd2.exe> [--selftest] [--test-bad-callsite] [--test-occupy]\n");
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
    if (!PatchWinMainCall(callSite, kVanillaWinMain, reinterpret_cast<void*>(&OracleMain), &error)) {
        std::fprintf(stderr, "oracle: refusing to patch the WinMain call: %s\n", error.c_str());
        return 6;
    }
    EnterImage(g_image);
}

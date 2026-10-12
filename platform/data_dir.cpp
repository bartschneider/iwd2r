// iwd2r. Distributed under LICENSE.md.
#define NOMINMAX
#include "platform/data_dir.h"

#include "platform/fs.h"
#include "platform/win32_api.h"

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <string>
#include <system_error>

namespace platform {

namespace {

    namespace stdfs = std::filesystem;

    std::string g_source;
    FatalMessageFn g_fatal = nullptr;

    void DefaultFatal(const char* text, const char* caption)
    {
        std::fprintf(stderr, "%s: %s\n", caption, text);
    }

    char Lower(char c)
    {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    }

    bool IsChitinKey(const std::string& name)
    {
        static const char key[] = "chitin.key";
        if (name.size() != sizeof(key) - 1)
            return false;
        for (size_t i = 0; i < name.size(); ++i) {
            if (Lower(name[i]) != key[i])
                return false;
        }
        return true;
    }

    bool HasChitinKey(const std::string& dir)
    {
        try {
            std::error_code ec;
            stdfs::path p(dir);
            if (!stdfs::is_directory(p, ec))
                return false;
            stdfs::directory_iterator it(p, ec), end;
            for (; !ec && it != end; it.increment(ec)) {
                if (IsChitinKey(it->path().filename().string()))
                    return true;
            }
        } catch (const std::exception&) {
        }
        return false;
    }

    std::string Child(const char* dir, const char* name)
    {
        try {
            return (stdfs::path(dir) / name).string();
        } catch (const std::exception&) {
            return std::string();
        }
    }

}

std::optional<DataDirResult> FindDataDir(const char* exeDir, const char* repoRootHint)
{
    auto accept = [](const std::string& path, const char* source) -> std::optional<DataDirResult> {
        if (HasChitinKey(path))
            return DataDirResult { path, source };
        return std::nullopt;
    };

    const char* env = std::getenv("IWD2_DATA");
    if (env && *env) {
        if (auto r = accept(env, "env"))
            return r;
    }
    if (exeDir && *exeDir) {
        if (auto r = accept(Child(exeDir, "IWD2"), "exe-link"))
            return r;
    }
    if (repoRootHint && *repoRootHint) {
        if (auto r = accept(Child(repoRootHint, "IWD2"), "repo-link"))
            return r;
    }
    std::error_code ec;
    stdfs::path cwd = stdfs::current_path(ec);
    if (!ec) {
        try {
            if (auto r = accept(cwd.string(), "cwd"))
                return r;
        } catch (const std::exception&) {
        }
    }
    return std::nullopt;
}

bool ResolveDataDir()
{
    std::string exeDir;
    std::string repoHint;
    char buffer[4096];
    DWORD n = GetModuleFileNameA(nullptr, buffer, sizeof(buffer));
    if (n > 0 && n < sizeof(buffer)) {
        try {
            stdfs::path dir = stdfs::path(std::string(buffer, n)).parent_path();
            exeDir = dir.string();
            if (dir.has_parent_path())
                repoHint = dir.parent_path().parent_path().string();
        } catch (const std::exception&) {
        }
    }
    return ResolveDataDirFrom(exeDir.c_str(), repoHint.c_str());
}

bool ResolveDataDirFrom(const char* exeDir, const char* repoRootHint)
{
    auto found = FindDataDir(exeDir, repoRootHint);
    if (!found) {
        FatalMessageFn fn = g_fatal ? g_fatal : DefaultFatal;
        fn("Set IWD2_DATA to your Icewind Dale II install directory (the one containing CHITIN.KEY).", "iwd2r");
        return false;
    }

    fs::SetDataRoot(found->path);
    fs::InstallResolver();
    SetCurrentDirectoryA(found->path.c_str());
    g_source = found->source;
    return true;
}

const char* DataDirSource()
{
    return g_source.c_str();
}

void SetFatalMessageHandler(FatalMessageFn handler)
{
    g_fatal = handler;
}

}

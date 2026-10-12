// iwd2r. Distributed under LICENSE.md.
// Locates the player's own Icewind Dale II install (the directory holding CHITIN.KEY).
#pragma once

#include <optional>
#include <string>

namespace platform {

struct DataDirResult {
    std::string path;
    std::string source; // "env", "exe-link", "repo-link" or "cwd"
};

// Tries IWD2_DATA, exeDir/IWD2, repoRootHint/IWD2, then the current directory.
// A null or empty directory skips that candidate.
std::optional<DataDirResult> FindDataDir(const char* exeDir, const char* repoRootHint);

// Finds the data directory, then sets the data root, installs the path resolver and
// changes the current directory to it. On failure reports through the fatal handler.
bool ResolveDataDir();

// Same as ResolveDataDir, with the executable directory and repo hint supplied by the caller.
bool ResolveDataDirFrom(const char* exeDir, const char* repoRootHint);

// The source chosen by the last successful ResolveDataDir; empty before.
const char* DataDirSource();

using FatalMessageFn = void (*)(const char* text, const char* caption);
// Reports a start-up failure through the installed handler (or the default).
void ReportFatal(const char* text, const char* caption);
// Passing null restores the default (prints "caption: text" to stderr).
void SetFatalMessageHandler(FatalMessageFn handler);

}

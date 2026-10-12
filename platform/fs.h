// iwd2r. Distributed under LICENSE.md.
// Path resolution for every game file access: backslashes, drive-letter
// re-rooting under the data root, and case-insensitive component lookup.
#pragma once

#include <string>

namespace platform::fs {

// The directory that holds the game data; empty disables re-rooting and folding.
void SetDataRoot(const std::string& root);
const std::string& DataRoot();

// The Windows install prefix (for example "C:\Games\IWD2\"), or empty.
void SetInstallPrefix(const std::string& windowsPrefix);

std::string Resolve(const char* path);

// Case-insensitive match with '*' (any run) and '?' (one character).
bool WildcardMatch(const char* pattern, const char* name);

// Forgets cached directory listings.
void InvalidateCache();

// Installs Resolve as the compat path resolver.
void InstallResolver();

}

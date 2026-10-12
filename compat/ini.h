// iwd2r. Distributed under LICENSE.md.
// Win32 private profile (INI) functions. File names pass through
// compat::ResolvePath; documents are cached per resolved path.
//
// IWD2R_INI_OVERRIDE ("Section/Key=Value;Section/Key=Value", read once on
// first use) overrides reads: GetPrivateProfileStringA and
// GetPrivateProfileIntA return the value for a matching section and key
// (case-insensitive) whatever the file, and never write it to disk. Writes
// and enumeration (null section or key) are unaffected.
#pragma once

#include "win_types.h"

DWORD GetPrivateProfileStringA(LPCSTR lpAppName, LPCSTR lpKeyName, LPCSTR lpDefault, LPSTR lpReturnedString, DWORD nSize, LPCSTR lpFileName);
UINT GetPrivateProfileIntA(LPCSTR lpAppName, LPCSTR lpKeyName, INT nDefault, LPCSTR lpFileName);
BOOL WritePrivateProfileStringA(LPCSTR lpAppName, LPCSTR lpKeyName, LPCSTR lpString, LPCSTR lpFileName);

namespace compat::ini {

// Drops every cached document; the next access re-reads the file.
void FlushCache();

// Forgets the parsed IWD2R_INI_OVERRIDE; the next read parses it again.
void ResetOverridesForTesting();

}

// iwd2r. Distributed under LICENSE.md.
// Win32 private profile (INI) functions. File names pass through
// compat::ResolvePath; documents are cached per resolved path.
#pragma once

#include "win_types.h"

DWORD GetPrivateProfileStringA(LPCSTR lpAppName, LPCSTR lpKeyName, LPCSTR lpDefault, LPSTR lpReturnedString, DWORD nSize, LPCSTR lpFileName);
UINT GetPrivateProfileIntA(LPCSTR lpAppName, LPCSTR lpKeyName, INT nDefault, LPCSTR lpFileName);
BOOL WritePrivateProfileStringA(LPCSTR lpAppName, LPCSTR lpKeyName, LPCSTR lpString, LPCSTR lpFileName);

namespace compat::ini {

// Drops every cached document; the next access re-reads the file.
void FlushCache();

}

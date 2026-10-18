# IEexHelper DLL scope — fork vs upstream (2026-10-18)

Both DLLs imported into their own analysed Ghidra project and measured with
`tools/ghidra/DllScope.java` (run through
`tools/ghidra/make_dll_project.py`'s `run_script`, under the ghidra lock):

    python3 -I tools/ghidra/make_dll_project.py ieexhelper-fork \
        $IWD2R_HOME/wso/dll/IEexHelper.fork.dll
    python3 -I tools/ghidra/make_dll_project.py ieexhelper-upstream \
        $IWD2R_HOME/wso/dll/IEexHelper.upstream.dll

`DLLSCOPE IEexHelper.fork.dll`:

    DLLSCOPE functions=1585 code_bytes=353902 imports=242 gl_imports=0 exports=13 ieex_helper_exports=0
    DLLSCOPE libs: API-MS-WIN-CRT-CONVERT-L1-1-0.DLL API-MS-WIN-CRT-FILESYSTEM-L1-1-0.DLL API-MS-WIN-CRT-HEAP-L1-1-0.DLL API-MS-WIN-CRT-MATH-L1-1-0.DLL API-MS-WIN-CRT-RUNTIME-L1-1-0.DLL API-MS-WIN-CRT-STDIO-L1-1-0.DLL API-MS-WIN-CRT-STRING-L1-1-0.DLL API-MS-WIN-CRT-UTILITY-L1-1-0.DLL DBGHELP.DLL KERNEL32.DLL LUA51.DLL MSVCP140.DLL SHLWAPI.DLL USER32.DLL VCRUNTIME140.DLL _Lockit basic_ios<char,std::char_traits<char>_> basic_istream<char,std::char_traits<char>_> basic_ostream<char,std::char_traits<char>_> basic_streambuf<char,std::char_traits<char>_> codecvt<char,char,_Mbstatet> codecvt_base id ios_base locale std
    DLLSCOPE gl:
    DLLSCOPE render_helpers:

`DLLSCOPE IEexHelper.upstream.dll`:

    DLLSCOPE functions=1298 code_bytes=236197 imports=182 gl_imports=0 exports=13 ieex_helper_exports=0
    DLLSCOPE libs: API-MS-WIN-CRT-CONVERT-L1-1-0.DLL API-MS-WIN-CRT-FILESYSTEM-L1-1-0.DLL API-MS-WIN-CRT-HEAP-L1-1-0.DLL API-MS-WIN-CRT-MATH-L1-1-0.DLL API-MS-WIN-CRT-RUNTIME-L1-1-0.DLL API-MS-WIN-CRT-STRING-L1-1-0.DLL API-MS-WIN-CRT-UTILITY-L1-1-0.DLL DBGHELP.DLL KERNEL32.DLL LUA51.DLL MSVCP140.DLL SHLWAPI.DLL USER32.DLL VCRUNTIME140.DLL _Lockit basic_ios<char,std::char_traits<char>_> basic_istream<char,std::char_traits<char>_> basic_ostream<char,std::char_traits<char>_> basic_streambuf<char,std::char_traits<char>_> codecvt<char,char,_Mbstatet> codecvt_base id ios_base locale std
    DLLSCOPE gl:
    DLLSCOPE render_helpers:

The function counts and code bytes match the numbers staged with the wso
drop (fork 1585/353902, upstream 1298/236197). Neither DLL imports gl*/wgl*
directly (`gl_imports=0`) — the GL calls must go through the main exe — and
neither exports an `IEex_Helper_*` symbol; both export exactly 13 symbols
(the module's own entry points). The fork has 287 more functions and
117,705 more bytes of code, and imports from two libraries upstream does
not: API-MS-WIN-CRT-STDIO-L1-1-0.DLL and SHLWAPI.DLL.

## Delta: fork functions with no upstream counterpart

`tools/ghidra/DllDelta.java` (fork project against the upstream project —
same referenced string literals, same imported calls, size within 25%):

    DLLDELTA 128 functions with no counterpart

The full listing lives next to the fork project
(`$IWD2R_HOME/ghidra-projects/ieexhelper-fork/dll-delta.tsv`, not
committed); the row count is the durable fact here. The rows are
overwhelmingly functions whose strings name game-side symbols
(`CVidCell::*Original`, `CResWED::*`, Lua-binding boilerplate), i.e. the
fork's rendering and hook surface.

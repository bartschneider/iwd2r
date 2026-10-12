// iwd2r. Distributed under LICENSE.md.
// Aggregator for the MFC and Win32 compatibility layer, standing in for the
// engine's afxwin.h style includes.
#pragma once

// win_types.h defines the Windows-style min/max macros (unless NOMINMAX), which
// break libstdc++ headers included after them. Pull in the standard headers the
// shims and the engine use first, so later includes of them are no-ops.
#include <algorithm>
#include <bitset>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "win_types.h"
#include "mfc_string.h"
#include "mfc_geometry.h"
#include "mfc_list.h"
#include "mfc_array.h"
#include "mfc_map.h"
#include "mfc_time.h"
#include "mfc_file.h"
#include "mfc_sync.h"
#include "mfc_wnd.h"
#include "mfc_resource.h"
#include "ini.h"
#include "path_hook.h"
#include "platform/win32_api.h"
#include "mfc_filefind.h"

#define ASSERT(x) ((void)0)
#define TRACE(...) ((void)0)
#define VERIFY(x) ((void)(x))
#define AFX_INLINE inline

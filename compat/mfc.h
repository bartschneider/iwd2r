// iwd2r. Distributed under LICENSE.md.
// Aggregator for the MFC and Win32 compatibility layer, standing in for the
// engine's afxwin.h style includes.
#pragma once

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
#include "path_hook.h"
#include "platform/win32_api.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define ASSERT(x) ((void)0)
#define TRACE(...) ((void)0)
#define VERIFY(x) ((void)(x))
#define AFX_INLINE inline

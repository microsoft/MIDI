// Copyright (c) Microsoft Corporation. All rights reserved.

#include <windows.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Data.Json.h>

using namespace winrt;

#include <wrl\module.h>
#include <wrl\event.h>
#include <avrt.h>
#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <wil\tracelogging.h>

#include <mmsystem.h>
#include <mmddk.h>
#include <strsafe.h>

#include <SDKDDKVer.h>

#define _ATL_APARTMENT_THREADED
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS // some CString constructors will be explicit
#define ATL_NO_ASSERT_ON_DESTROY_NONEXISTENT_WINDOW

#include "resource.h"
#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>
#include <atlcoll.h>
#include <atlsync.h>

#include <queue>

#include <winmeta.h>
#include <TraceLoggingProvider.h>

#include <filesystem>

#include <Devpkey.h>
#include "MidiDefs.h"
#include "WindowsMidiServices.h"
#include "WindowsMidiServices_i.c"

#include "Midi2MidiSrvAbstraction.h"

#define HRESULT_FROM_MMRESULT(x) (x == MMSYSERR_NOERROR ? S_OK : MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, x))

MMRESULT MMRESULT_FROM_HRESULT(HRESULT hResult);

extern bool g_ProcessIsTerminating;


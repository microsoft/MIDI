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
#include <mmreg.h>
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

#include "Midi2MidiSrvTransport.h"

class WdmAud2TelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        WdmAud2TelemetryProvider,
        "Microsoft.Windows.Midi2.WdmAud2",
        // {e6443bc1-e9c5-5a3f-cfb6-abcd62e52e41}
        (0xe6443bc1,0xe9c5,0x5a3f,0xcf,0xb6,0xab,0xcd,0x62,0xe5,0x2e,0x41));

public:
};


#define HRESULT_FROM_MMRESULT(x) (x == MMSYSERR_NOERROR ? S_OK : MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, x))

MMRESULT MMRESULT_FROM_HRESULT(HRESULT hResult);

extern bool g_ProcessIsTerminating;


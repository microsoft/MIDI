// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

#ifndef STRICT
#define STRICT
#endif

#include <windows.h>

#include <hstring.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <assert.h>
#include <devioctl.h>
#include <wrl\implements.h>
#include <wrl\module.h>
#include <wrl\event.h>
//#include <ks.h>
//#include <ksmedia.h>
#include <avrt.h>
#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <wil\tracelogging.h>
#include <ppltasks.h>

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

#include <winmeta.h>
#include <TraceLoggingProvider.h>

#include "SWDevice.h"
#include <initguid.h>
#include "setupapi.h"
#include "Devpkey.h"

#include "strsafe.h"

#include "wstring_util.h"

// TransportUtilities
#include "wstring_util.h"
namespace internal = ::WindowsMidiServicesInternal;

#include "MidiDefs.h"
#include "WindowsMidiServices.h"
#include "WindowsMidiServices_i.c"

#include "json_defs.h"
#include "json_helpers.h"
#include "swd_helpers.h"
#include "resource_util.h"

#include "MidiXProc.h"

#include "transport_defs.h"

#include "Midi2DiagnosticsTransport_i.c"
#include "Midi2DiagnosticsTransport.h"

#include "dllmain.h"

#include "MidiDefs.h"
#include "MidiXProc.h"

class CMidi2DiagnosticsMidiConfigurationManager;

//#include "MidiLoopbackDevice.h"
#include "MidiLoopbackBidiDevice.h"
#include "MidiPingBidiDevice.h"

#include "Midi2.DiagnosticsTransport.h"
#include "Midi2.DiagnosticsEndpointManager.h"
#include "Midi2.DiagnosticsPluginMetadataProvider.h"
#include "Midi2.DiagnosticsEndpointManager.h"

//#include "Midi2.LoopbackMidiIn.h"
//#include "Midi2.LoopbackMidiOut.h"
#include "Midi2.LoopbackMidiBidi.h"
#include "Midi2.PingMidiBidi.h"

#include "Midi2.DiagnosticsMidiConfigurationManager.h"

#include "MidiDeviceTable.h"


// in the src\shared\inc folder in the repo but are duplicated into the API tree for internal reasons
#include "midi_timestamp.h"
#include "ping_ump_types.h"


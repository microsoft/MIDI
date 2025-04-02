// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


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

#include <vector>
#include <string>
#include <queue>

#include <mmsystem.h>
#include "SWDevice.h"
#include <initguid.h>
#include "setupapi.h"
#include "Devpkey.h"

#include "strsafe.h"


//#pragma push_macro("GetObject")
#undef GetObject
#include <winrt/Windows.Data.Json.h>
namespace json = ::winrt::Windows::Data::Json;
//#pragma pop_macro("GetObject")



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
#include "midi_group_terminal_blocks.h"
#include "midi_naming.h"

#include "MidiXProc.h"

#include "transport_defs.h"

#include "Midi2LoopbackMidiTransport_i.c"
#include "Midi2LoopbackMidiTransport.h"

#include "dllmain.h"


class CMidi2LoopbackMidiEndpointManager;
class CMidi2LoopbackMidiBidi;
class TransportState;

#include "MidiLoopbackDeviceDefinition.h"
#include "MidiLoopbackDevice.h"
#include "MidiLoopbackDeviceTable.h"

#include "Midi2.LoopbackMidiTransport.h"
#include "Midi2.LoopbackMidiBidi.h"
#include "Midi2.LoopbackMidiEndpointManager.h"
#include "Midi2.LoopbackMidiConfigurationManager.h"
#include "TransportWorkQueue.h"
#include "TransportState.h"
#include "Midi2.LoopbackMidiPluginMetadataProvider.h"


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
//#include <winrt/Windows.Data.Json.h>

//namespace json = ::winrt::Windows::Data::Json;


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

#include "mmdeviceapi.h"    // for E_NOTFOUND

#include <vector>
#include <string>

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

#include "virtual_midi_transport_defs.h"

#include "Midi2VirtualMidiTransport_i.c"
#include "Midi2VirtualMidiTransport.h"

#include "dllmain.h"


class CMidi2VirtualMidiEndpointManager;
class CMidi2VirtualMidiBidi;
class TransportState;

#include "MidiVirtualDeviceEndpointEntry.h"
#include "MidiEndpointTable.h"

#include "Midi2.VirtualMidiTransport.h"
#include "Midi2.VirtualMidiBidi.h"
#include "Midi2.VirtualMidiEndpointManager.h"
#include "Midi2.VirtualMidiConfigurationManager.h"
#include "TransportState.h"
#include "Midi2.VirtualMidiPluginMetadataProvider.h"

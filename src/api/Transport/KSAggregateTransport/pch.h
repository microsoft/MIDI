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

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>

#include <mmsystem.h>
#include <wrl\module.h>
#include <wrl\event.h>
#include <devioctl.h>
#include <ks.h>
#include <ksmedia.h>
#include <avrt.h>
#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <wil\tracelogging.h>
#include <wil\registry.h>
#include <wil\registry_helpers.h>

#include <SDKDDKVer.h>

#include <functional>


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

#include <atlconv.h>
#include <string>

#include <winmeta.h>
#include <TraceLoggingProvider.h>

#include "SWDevice.h"
#include <initguid.h>
#include "Devpkey.h"
#include <mmdeviceapi.h>

#include "wstring_util.h"
#include "midi_group_terminal_blocks.h"

namespace internal = ::WindowsMidiServicesInternal;

#include "MidiDefs.h"
#include "WindowsMidiServices.h"
#include "WindowsMidiServices_i.c"
#include "midi_utils.h"


#undef GetObject
#include <winrt/Windows.Data.Json.h>
namespace json = ::winrt::Windows::Data::Json;

#include "json_defs.h"
#include "json_helpers.h"
#include "json_custom_property_helper.h"

#include "swd_helpers.h"
#include "resource_util.h"
#include "ump_helpers.h"
#include "MidiXProc.h"

#include "midi_naming.h"


#include "strsafe.h"

#include "transport_defs.h"



#include "Midi2KSAggregateTransport_i.c"
#include "Midi2KSAggregateTransport.h"



#include "dllmain.h"

#include "MidiDefs.h"
#include "MidiKsDef.h"
#include "MidiKsCommon.h"
#include "MidiXProc.h"
#include "MidiKs.h"

#include "Midi2BS2UMPTransform.h"
#include "Midi2BS2UMPTransform_i.c"

#include "Midi2UMP2BSTransform.h"
#include "Midi2UMP2BSTransform_i.c"


class CMidi2KSAggregateMidiEndpointManager;
class CMidi2KSAggregateMidiInProxy;
class CMidi2KSAggregateMidiOutProxy;
class CMidi2KSAggregateMidiConfigurationManager;
class CMidi2KSAggregateMidiBidi;
class TransportState;

#include "Midi2.KSAggregateTransport.h"
#include "Midi2.KSAggregateMidiInProxy.h"
#include "Midi2.KSAggregateMidiOutProxy.h"
#include "Midi2.KSAggregateMidi.h"
#include "Midi2.KSAggregateMidiBidi.h"
#include "Midi2.KSAggregateMidiEndpointManager.h"
#include "Midi2.KSAggregateMidiConfigurationManager.h"
#include "Midi2.KSAggregateMidiPluginMetadataProvider.h"


#include "TransportState.h"


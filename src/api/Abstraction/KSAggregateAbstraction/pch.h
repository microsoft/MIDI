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

#include <wrl\module.h>
#include <wrl\event.h>
#include <ks.h>
#include <ksmedia.h>
#include <avrt.h>
#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <wil\tracelogging.h>

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


#undef GetObject
#include <winrt/Windows.Data.Json.h>
namespace json = ::winrt::Windows::Data::Json;

#include "json_defs.h"
#include "json_helpers.h"
#include "swd_helpers.h"
#include "resource_util.h"
#include "ump_helpers.h"

#include "MidiXProc.h"



#include "strsafe.h"

#include "abstraction_defs.h"



#include "Midi2KSAggregateAbstraction_i.c"
#include "Midi2KSAggregateAbstraction.h"



#include "dllmain.h"

#include "MidiDefs.h"
#include "MidiKsDef.h"
#include "MidiKsCommon.h"
#include "MidiXProc.h"
#include "MidiKs.h"

#include "Midi2BS2UMPTransform.h"
#include "Midi2BS2UMPTransform_i.c"


class CMidi2KSAggregateMidiEndpointManager;
class CMidi2KSAggregateMidiInProxy;
class CMidi2KSAggregateMidiConfigurationManager;
class CMidi2KSAggregateMidiBiDi;
class AbstractionState;

#include "Midi2.KSAggregateAbstraction.h"
#include "Midi2.KSAggregateMidiInProxy.h"
#include "Midi2.KSAggregateMidi.h"
#include "Midi2.KSAggregateMidiBiDi.h"
#include "Midi2.KSAggregateMidiEndpointManager.h"
#include "Midi2.KSAggregateMidiConfigurationManager.h"
#include "Midi2.KSAggregateMidiPluginMetadataProvider.h"


#include "AbstractionState.h"


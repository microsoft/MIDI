// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
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



// AbstractionUtilities
#include "wstring_util.h"
namespace internal = ::WindowsMidiServicesInternal;

#include "MidiDefs.h"
#include "MidiDataFormat.h"
#include "MidiFlow.h"
#include "MidiAbstraction.h"

#include "json_defs.h"
#include "json_helpers.h"
#include "swd_helpers.h"
#include "resource_util.h"

#include "MidiXProc.h"

#include "abstraction_defs.h"

#include "MidiServicePlugin.h"
#include "MidiServicePlugin_i.c"

#include "Midi2LoopbackMidiAbstraction_i.c"
#include "Midi2LoopbackMidiAbstraction.h"



#include "mididevicemanagerinterface_i.c"
#include "mididevicemanagerinterface.h"

#include "MidiEndpointProtocolManagerInterface_i.c"
#include "MidiEndpointProtocolManagerInterface.h"


#include "dllmain.h"


class CMidi2LoopbackMidiEndpointManager;
class CMidi2LoopbackMidiBiDi;
class AbstractionState;

#include "MidiLoopbackDeviceDefinition.h"
#include "MidiLoopbackDevice.h"
#include "MidiLoopbackDeviceTable.h"

#include "Midi2.LoopbackMidiAbstraction.h"
#include "Midi2.LoopbackMidiBiDi.h"
#include "Midi2.LoopbackMidiEndpointManager.h"
#include "Midi2.LoopbackMidiConfigurationManager.h"
#include "AbstractionState.h"
#include "Midi2.LoopbackMidiPluginMetadataProvider.h"

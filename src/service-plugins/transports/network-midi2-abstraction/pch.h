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
#include <winrt/Windows.Data.Json.h>

namespace json = ::winrt::Windows::Data::Json;


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
//#include "Devpkey.h"

#include "strsafe.h"
#include "wstring_util.h"

#undef GetObject
#include <winrt/Windows.Data.Json.h>
namespace json = ::winrt::Windows::Data::Json;


// AbstractionUtilities
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

#include "Midi2NetworkMidiAbstraction_i.c"
#include "Midi2NetworkMidiAbstraction.h"

#include "dllmain.h"

class CMidi2NetworkMidiEndpointManager;
class CMidi2NetworkMidiConfigurationManager;

#include "abstraction_defs.h"

#include "MidiNetworkDeviceDefinition.h"
#include "MidiNetworkDevice.h"
#include "MidiNetworkDeviceTable.h"

#include "AbstractionState.h"

#include "Midi2.NetworkMidiAbstraction.h"
#include "Midi2.NetworkMidiIn.h"
#include "Midi2.NetworkMidiOut.h"
#include "Midi2.NetworkMidiBiDi.h"
#include "Midi2.NetworkMidiEndpointManager.h"
#include "Midi2.NetworkMidiConfigurationManager.h"
#include "Midi2.NetworkMidiPluginMetadataProvider.h"


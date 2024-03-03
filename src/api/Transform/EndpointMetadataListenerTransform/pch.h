// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

#ifndef STRICT
#define STRICT
#endif

#include <windows.h>
#include <queue>

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
#include "plugin_defs.h"
#include "midi_timestamp.h"
#include "midi_ump.h"
#include "ump_helpers.h"
#include "midi_ump_message_defs.h"

namespace internal = ::Windows::Devices::Midi2::Internal;
namespace shared = ::Windows::Devices::Midi2::Internal::Shared;

#include "mididevicemanagerinterface_i.c"
#include "mididevicemanagerinterface.h"

#include "MidiServicePlugin.h"
#include "MidiServicePlugin_i.c"

#include "Midi2EndpointMetadataListenerTransform_i.c"
#include "Midi2EndpointMetadataListenerTransform.h"


#include "dllmain.h"

#include "MidiDefs.h"
#include "MidiXProc.h"


#include "Midi2.EndpointMetadataListenerTransform.h"
#include "Midi2.EndpointMetadataListenerMidiTransform.h"


// in the src\shared\inc folder in the repo but are duplicated into the API tree for internal reasons



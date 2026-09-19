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
#include <avrt.h>
// Must precede the other wil headers: without it WIL cannot identify winrt::hresult_error
// and fail fasts instead of logging, turning every catch site into a process crash.
#include <wil\cppwinrt.h>
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

#include <atomic>
#include <memory>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include <mmsystem.h>
#include "SWDevice.h"
#include <initguid.h>
#include "setupapi.h"
#include "Devpkey.h"

#include "strsafe.h"

#include <mmdeviceapi.h>    // for GUID definitions for MIDI Input/Output

// windows.h defines GetObject as a macro, which collides with a method on the JSON projection.
#undef GetObject
#include <winrt/Windows.Data.Json.h>
namespace json = ::winrt::Windows::Data::Json;

#include "Feature_Servicing_MIDI2EndpointNameUtf8ByteLimit.h"

// TransportUtilities
#include "wstring_util.h"
namespace internal = ::WindowsMidiServicesInternal;

#include "MidiDefs.h"
#include "midi_ump_message_defs.h"
#include "midi_timestamp.h"
#include "ump_helpers.h"
#include "WindowsMidiServices.h"
#include "WindowsMidiServices_i.c"

#include "json_defs.h"
#include "json_helpers.h"
#include "swd_helpers.h"
#include "resource_util.h"
#include "midi_group_terminal_blocks.h"

#include "MidiXProc.h"

// The synthesizer engine. Host agnostic: no COM, no WinRT, no SDK, so the same code runs in the
// offline test tools and in this transport.
#include "MidiSynth/DlsCollection.h"
#include "MidiSynth/ProgramList.h"
#include "MidiSynth/PropertyExchangeSource.h"
#include "MidiSynth/SpscRingBuffer.h"
#include "MidiSynth/SynthConfig.h"
#include "MidiSynth/SynthEngine.h"
#include "MidiSynth/UmpDispatcher.h"
#include "MidiSynth/UmpRenderSource.h"
#include "MidiSynth/AudioSink.h"

#include "midi_synth_transport_defs.h"

#include "Midi2MidiSynthTransport_i.c"
#include "Midi2MidiSynthTransport.h"

#include "dllmain.h"


class CMidi2MidiSynthEndpointManager;
class CMidi2MidiSynthConfigurationManager;
class CMidi2MidiSynthBidi;
class TransportState;

#include "Midi2.MidiSynthTransport.h"

#include "MidiSynthSettings.h"
#include "MidiSynthDevice.h"

#include "Midi2.MidiSynthBidi.h"
#include "Midi2.MidiSynthConfigurationManager.h"
#include "Midi2.MidiSynthEndpointManager.h"
#include "Midi2.MidiSynthPluginMetadataProvider.h"

// Last: it holds com_ptr members of the classes above, which have to be complete by then.
#include "TransportState.h"

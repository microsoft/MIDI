// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <windows.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>


#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>

#include <string>
#include <iterator>
#include <strsafe.h>
#include <wrl\module.h>
#include <wrl\event.h>
#include <avrt.h>
#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <wil\win32_helpers.h>
#include <wil\registry.h>
#include <wil\result.h>
#include <wil\tracelogging.h>
#include <wil\wistd_memory.h>
#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>
#include <atlcoll.h>
#include <atlsync.h>
#include <sddl.h>

#include "SWDevice.h"
#include <initguid.h>
#include <MMDeviceAPI.h>
#include "devpkey.h"

// Shared helpers
#include "midi_ump.h"
#include "midi_timestamp.h"

// AbstractionUtilities
#include "wstring_util.h"
namespace internal = ::Windows::Devices::Midi2::Internal;

#include "MidiDefs.h"
#include "MidiDataFormat.h"
#include "MidiFlow.h"
#include "MidiAbstraction.h"

#include "json_defs.h"
#include "json_helpers.h"
#include "swd_helpers.h"

namespace shared = ::Windows::Devices::Midi2::Internal::Shared;

#include "resource.h"
#include "propkey.h"

// Abstractions. Only the Diagnostics abstraction is known to the service
// All the others are discovered through COM

#include "Midi2DiagnosticsAbstraction_i.c"
#include "Midi2DiagnosticsAbstraction.h"

//#include "Midi2NetworkMidiAbstraction.h"
//#include "Midi2VirtualMidiAbstraction.h"
//#include "Midi2KSAbstraction.h"

// Base MIDI interfaces

#include "MidiDataFormat.h"
#include "MidiFlow.h"

#include "MidiAbstraction.h"
#include "MidiAbstraction_i.c"

#include "MidiServicePlugin.h"
#include "MidiServicePlugin_i.c"

#include "MidiDeviceManagerInterface_i.c"
#include "MidiDeviceManagerInterface.h"

#include "MidiEndpointProtocolManagerInterface_i.c"
#include "MidiEndpointProtocolManagerInterface.h"

// RPC Calls

#include "MidiSrvRpc.h"

// Transforms

#include "Midi2BS2UMPTransform.h"
#include "Midi2UMP2BSTransform.h"
#include "Midi2SchedulerTransform.h"
#include "Midi2EndpointMetadataListenerTransform.h"


// MidiSrv internal classes

class CMidiEndpointProtocolManager;
struct GUIDCompare;
class CMidiSessionTracker;

#include "MidiTelemetry.h"
#include "MidiPerformanceManager.h"
#include "MidiProcessManager.h"
#include "MidiConfigurationManager.h"
#include "MidiDeviceManager.h"
#include "MidiXProc.h"

// MidiDevicePipe holds MidiClientPipe(s) that it is connected to.
// MidiClientPipe holds a MidiDevicePipe that it is connected to.
// declare these prior to including the headers.
class CMidiClientPipe;
class CMidiDevicePipe;

#include "MidiDevicePipe.h"
#include "MidiClientPipe.h"
#include "MidiTransformPipe.h"
#include "MidiClientManager.h"

#include "MidiEndpointProtocolManager.h"
#include "MidiSessionTracker.h"

#include "MidiSrv.h"



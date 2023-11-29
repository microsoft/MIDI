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

#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>

namespace json = ::winrt::Windows::Data::Json;

#include <string>
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

#include "midi_ump.h"
#include "midi_timestamp.h"

namespace internal = ::Windows::Devices::Midi2::Internal;
namespace shared = ::Windows::Devices::Midi2::Internal::Shared;

#include "MidiAbstraction.h"

#include "resource.h"
#include "propkey.h"

#include "Midi2DiagnosticsAbstraction_i.c"
#include "Midi2DiagnosticsAbstraction.h"

#include "Midi2NetworkMidiAbstraction.h"
#include "Midi2VirtualMidiAbstraction.h"
#include "Midi2KSAbstraction.h"

#include "mididevicemanagerinterface_i.c"
#include "mididevicemanagerinterface.h"

#include "MidiSrvRpc.h"

#include "SWDevice.h"
#include <initguid.h>
#include <MMDeviceAPI.h>
#include <Devpkey.h>
#include "MidiDefs.h"


// MidiDevicePipe holds MidiClientPipe(s) that it is connected to.
// MidiClientPipe holds a MidiDevicePipe that it is connected to.
// declare these prior to including the headers.
class CMidiClientPipe;
class CMidiDevicePipe;
class CMidiTransformPipe;

// transform manager takes a ref to client manager and device manager
class CMidiClientManager;

#include "MidiTelemetry.h"
#include "MidiPerformanceManager.h"
#include "MidiProcessManager.h"
#include "MidiConfigurationManager.h"
#include "MidiDeviceManager.h"
#include "MidiXProc.h"



#include "MidiDevicePipeMessageScheduler.h"

#include "MidiDevicePipe.h"
#include "MidiClientPipe.h"
#include "MidiClientManager.h"


#include "MidiSrv.h"



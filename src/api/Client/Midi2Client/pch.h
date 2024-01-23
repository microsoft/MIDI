// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once

//#define _VSDESIGNER_DONT_LOAD_AS_DLL


#include <unknwn.h>

#include <Windows.h>

//#include <wil/cppwinrt.h> // must be before the first C++ WinRT header
//#include <wil/result.h>


#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Storage.h>

#include <winrt/Windows.Data.Json.h>


#include <stdint.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <string>
#include <cwctype>


// internal
#include "trace_logging.h"

#include "midi_service_interface.h"
#include "ump_helpers.h"
#include "memory_buffer.h"
#include "string_util.h"

// shared
#include "midi_ump.h"   // general shared
#include "loopback_ids.h"
#include <midi_timestamp.h>

//#include <wil/resource.h>

namespace foundation = ::winrt::Windows::Foundation;
namespace collections = ::winrt::Windows::Foundation::Collections;

namespace internal = ::Windows::Devices::Midi2::Internal;

#include "resource.h"
#include "ResourceManager.h"

#include "MidiGroup.h"
#include "MidiChannel.h"
#include "MidiUniqueId.h"

// TODO: Consider changing these to midi2impl and midi2proj
namespace implementation = winrt::Windows::Devices::Midi2::implementation;
namespace midi2 = ::winrt::Windows::Devices::Midi2;



#include "midi_stream_message_defs.h"
#include "midi_ump_message_defs.h"

#include <Devpropdef.h>
#include "MidiDefs.h"
#include "midi_function_block_prop_util.h"


#include "MidiMessage32.h"
#include "MidiMessage64.h"
#include "MidiMessage96.h"
#include "MidiMessage128.h"

#include "MidiFunctionBlock.h"
#include "MidiGroupTerminalBlock.h"

#include "MidiEndpointConnection.h"

#include "MidiEndpointConnectionOptions.h"

#include "MidiMessageReceivedEventArgs.h"
#include "MidiEndpointDeviceInformationUpdateEventArgs.h"

#include "MidiVirtualEndpointDevice.h"
#include "MidiVirtualEndpointDeviceDefinition.h"

#include "MidiSession.h"

#include "MidiServicePingResponse.h"
#include "MidiServicePingResponseSummary.h"
#include "MidiTransportPluginInformation.h"
#include "MidiMessageProcessingPluginInformation.h"
#include "MidiService.h"

#include "MidiEndpointDeviceInformation.h"
#include "MidiEndpointDeviceWatcher.h"

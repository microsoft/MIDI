// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

//#define _VSDESIGNER_DONT_LOAD_AS_DLL



#include <Windows.h>

#include <optional>
#include <stdint.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <string>
#include <cwctype>
#include <queue>
#include <mutex>
#include <format>
#include <filesystem>


// wil\cppwinrt.h must be included before any other WinRT includes.
#include <wil\cppwinrt.h>
#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <wil\tracelogging.h>
#include <wil\registry.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Devices.Midi.h>
#include <winrt/Windows.Web.Http.h>

#include <winrt/Windows.Devices.Enumeration.Pnp.h>


#undef GetObject

#include <winrt/Windows.Data.Json.h>
namespace json =            ::winrt::Windows::Data::Json;
namespace enumeration =     ::winrt::Windows::Devices::Enumeration;
namespace midi1 =           ::winrt::Windows::Devices::Midi;
namespace foundation =      ::winrt::Windows::Foundation;
namespace collections =     ::winrt::Windows::Foundation::Collections;
namespace streams =         ::winrt::Windows::Storage::Streams;
namespace coreapp =         ::winrt::Windows::ApplicationModel::Core;
namespace pnp =             ::winrt::Windows::Devices::Enumeration::Pnp;        // needed to get the parent device id for an interface. Normal enum doesn't project it.

// pre-declare namespaces
namespace WindowsMidiServicesInternal {};
namespace internal =        ::WindowsMidiServicesInternal;

#include <winrt/Windows.Devices.Midi2.h>
namespace midi2 = ::winrt::Windows::Devices::Midi2;

#include <mmsyscom.h>   // needed for MAXPNAMELEN

#include "devpkey.h"
#include <Devpropdef.h>
#include <cfgmgr32.h>   // needed for CM_Locate_DevNodeW / CM_Get_DevNode_PropertyW (parent lookup)
#include <MidiDefs.h>
#include <midi_group_terminal_blocks.h>

// shared
#include <midi_ump.h>   // general shared
#include <loopback_ids.h>
#include <midi_timestamp.h>
#include <json_defs.h>
#include <ping_ump_types.h>
#include <ump_helpers.h>
#include <hstring_util.h>
#include <wstring_util.h>
#include <json_helpers.h>
#include <resource_util.h>
#include <swd_helpers.h>
#include <midi_ump_message_defs.h>

#include "winrt_date_util.h"
#include "winrt_devpkey_util.h"
#include "cfgmgr_helpers.h"

#include "MidiPnpDeviceInfo.h"

// service interface
//#include <WindowsMidiServices.h>
//#include <WindowsMidiServices_i.c>
//#include <Midi2MidiSrvTransport.h>

// SDK shared
#include <SdkTraceLogging.h>

// Project-local ------------------------------------------------

namespace winrt::Windows::Devices::Midi2::Enumeration {};
namespace midi2enum = ::winrt::Windows::Devices::Midi2::Enumeration;

namespace winrt::Windows::Devices::Midi2::Enumeration::Legacy {};
namespace legacy = ::winrt::Windows::Devices::Midi2::Enumeration::Legacy;

#include <safe_cotaskmemfree.h>


#include "resource.h"

#include "midi_stream_message_defs.h"
#include "midi_function_block_prop_util.h"
#include "endpoint_device_interface_helpers.h"
#include "winrt_enumeration_prop_util.h"
#include "MidiEndpointNametable.h"

#include "MidiFunctionBlock.h"
#include "MidiGroupTerminalBlock.h"
#include "MidiEndpointTransportSuppliedInfo.h"
#include "MidiEndpointUserSuppliedInfo.h"
#include "MidiDeclaredDeviceIdentity.h"
#include "MidiDeclaredEndpointInfo.h"
#include "MidiDeclaredStreamConfiguration.h"
#include "Midi1PortNameTableEntry.h"

#include "MidiLegacyPortDeviceInformation.h"
#include "MidiLegacyPortDeviceInformationAddedEventArgs.h"
#include "MidiLegacyPortDeviceInformationUpdatedEventArgs.h"
#include "MidiLegacyPortDeviceInformationRemovedEventArgs.h"
#include "MidiLegacyPortDeviceWatcher.h"

#include "MidiEndpointDeviceInformation.h"
#include "MidiEndpointDeviceInformationAddedEventArgs.h"
#include "MidiEndpointDeviceInformationUpdatedEventArgs.h"
#include "MidiEndpointDeviceInformationRemovedEventArgs.h"
#include "MidiEndpointDeviceWatcher.h"




#include "MidiEndpointDevicePropertyHelper.h"

#include <shared_mutex>
//#include "MidiImageAssetHelper.h"

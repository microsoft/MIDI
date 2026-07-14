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
#include <winrt/Windows.Devices.Enumeration.Pnp.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Devices.Midi.h>
#include <winrt/Windows.Web.Http.h>


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




#include <mmsyscom.h>   // needed for MAXPNAMELEN

#include "devpkey.h"
#include <Devpropdef.h>
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
#include "async_helper.h"
#include "MidiPnpDeviceInfo.h"
#include "MidiPnpUtilities.h"

#include "winrt_enumeration_prop_util.h"

#include "midi_stream_message_defs.h"
#include "midi_function_block_prop_util.h"
#include "endpoint_device_interface_helpers.h"

// service interface
#include <WindowsMidiServices.h>
#include <WindowsMidiServices_i.c>
#include <Midi2MidiSrvTransport.h>
#include <Midi2MidiSrvTransport_i.c>

// SDK shared
#include <SdkTraceLogging.h>

// Project-local ------------------------------------------------

namespace winrt::Windows::Devices::Midi2 {};
namespace midi2 = ::winrt::Windows::Devices::Midi2;

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry {};
namespace ci = ::winrt::Windows::Devices::Midi2::CapabilityInquiry;

namespace winrt::Windows::Devices::Midi2::Diagnostics {};
namespace diag = ::winrt::Windows::Devices::Midi2::Diagnostics;

namespace winrt::Windows::Devices::Midi2::Enumeration {};
namespace midi2enum = ::winrt::Windows::Devices::Midi2::Enumeration;

namespace winrt::Windows::Devices::Midi2::Enumeration::Legacy {};
namespace legacy = ::winrt::Windows::Devices::Midi2::Enumeration::Legacy;

namespace winrt::Windows::Devices::Midi2::Reporting {};
namespace rpt = ::winrt::Windows::Devices::Midi2::Reporting;

namespace winrt::Windows::Devices::Midi2::ServiceConfig {};
namespace svc = ::winrt::Windows::Devices::Midi2::ServiceConfig;

namespace winrt::Windows::Devices::Midi2::Transports::Loopback {};
namespace loop = ::winrt::Windows::Devices::Midi2::Transports::Loopback;

namespace winrt::Windows::Devices::Midi2::Transports::BasicLoopback {};
namespace bloop = ::winrt::Windows::Devices::Midi2::Transports::BasicLoopback;

namespace winrt::Windows::Devices::Midi2::Transports::Virtual {};
namespace virt = ::winrt::Windows::Devices::Midi2::Transports::Virtual;

namespace winrt::Windows::Devices::Midi2::Utilities::Messages {};
namespace msgs = ::winrt::Windows::Devices::Midi2::Utilities::Messages;




#define SAFE_COTASKMEMFREE(p) \
    if (NULL != p) { \
        CoTaskMemFree(p); \
        (p) = NULL; \
    }


// libmidi2 from vcpkg

#pragma warning(push)
#pragma warning(disable: 4996)
#include <libmidi2/bytestreamToUMP.h>
#include <libmidi2/umpToBytestream.h>
#pragma warning(pop)



//#include <WindowsMidiServicesSdkRuntimeVersion.h>

#include "resource.h"

#include "MidiEndpointNametable.h"

#include "winrt_memory_buffer.h"

#include <shared_mutex>

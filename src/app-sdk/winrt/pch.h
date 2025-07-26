// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
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


#include <winrt/Windows.Data.Json.h>
namespace json = ::winrt::Windows::Data::Json;
namespace enumeration = ::winrt::Windows::Devices::Enumeration;
namespace midi1 = ::winrt::Windows::Devices::Midi;
namespace foundation = ::winrt::Windows::Foundation;
namespace collections = ::winrt::Windows::Foundation::Collections;
namespace streams = ::winrt::Windows::Storage::Streams;

namespace coreapp = ::winrt::Windows::ApplicationModel::Core;

// pre-declare namespaces

namespace WindowsMidiServicesInternal {};
namespace winrt::Microsoft::Windows::Devices::Midi2 {};

namespace internal = ::WindowsMidiServicesInternal;
namespace midi2 = ::winrt::Microsoft::Windows::Devices::Midi2;


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

#include "date_util.h"

// service interface
#include <WindowsMidiServices.h>
#include <WindowsMidiServices_i.c>
#include <Midi2MidiSrvTransport.h>

// SDK shared
#include <SdkTraceLogging.h>

// Project-local ------------------------------------------------

namespace winrt::Microsoft::Windows::Devices::Midi2 {};
namespace midi2 = ::winrt::Microsoft::Windows::Devices::Midi2;

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Loopback {};
namespace loop = ::winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Loopback;

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Virtual {};
namespace virt = ::winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Virtual;

namespace winrt::Microsoft::Windows::Devices::Midi2::VirtualPatchBay {};
namespace vpb = ::winrt::Microsoft::Windows::Devices::Midi2::VirtualPatchBay;

namespace winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig {};
namespace svc = ::winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig;

namespace winrt::Microsoft::Windows::Devices::Midi2::Diagnostics {};
namespace diag = ::winrt::Microsoft::Windows::Devices::Midi2::Diagnostics;

namespace winrt::Microsoft::Windows::Devices::Midi2::Reporting {};
namespace rept = ::winrt::Microsoft::Windows::Devices::Midi2::Reporting;

namespace winrt::Microsoft::Windows::Devices::Midi2::Utilities::SysExTransfer {};
namespace sysex = ::winrt::Microsoft::Windows::Devices::Midi2::Utilities::SysExTransfer;

namespace winrt::Microsoft::Windows::Devices::Midi2::Messages {};
namespace msgs = ::winrt::Microsoft::Windows::Devices::Midi2::Messages;

namespace winrt::Microsoft::Windows::Devices::Midi2::CapabilityInquiry {};
namespace ci = ::winrt::Microsoft::Windows::Devices::Midi2::CapabilityInquiry;

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network {};
namespace network = ::winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network;


#define SAFE_COTASKMEMFREE(p) \
    if (NULL != p) { \
        CoTaskMemFree(p); \
        (p) = NULL; \
    }

#include <WindowsMidiServicesSdkRuntimeVersion.h>

#include "resource.h"

#include "midi_stream_message_defs.h"
#include "midi_function_block_prop_util.h"
#include "endpoint_device_interface_helpers.h"
#include "enumeration_prop_util.h"
#include "midi_naming.h"


#include "memory_buffer.h"

#include "MidiGroup.h"
#include "MidiChannel.h"
#include "MidiUniqueId.h"
#include "MidiUniversalSystemExclusiveChannel.h"

#include "MidiClock.h"

#include "MidiMessage32.h"
#include "MidiMessage64.h"
#include "MidiMessage96.h"
#include "MidiMessage128.h"

#include "MidiStreamMessageBuilder.h"
#include "MidiMessageBuilder.h"
#include "MidiMessageHelper.h"

#include "MidiFunctionBlock.h"
#include "MidiGroupTerminalBlock.h"

#include "MidiEndpointConnectionBasicSettings.h"
#include "MidiEndpointConnection.h"

#include "MidiMessageReceivedEventArgs.h"

#include "MidiEndpointAssociatedPortDeviceInformation.h"

#include "MidiEndpointDeviceInformation.h"
#include "MidiEndpointDeviceInformationAddedEventArgs.h"
#include "MidiEndpointDeviceInformationUpdatedEventArgs.h"
#include "MidiEndpointDeviceInformationRemovedEventArgs.h"
#include "MidiEndpointDeviceWatcher.h"

#include "MidiSession.h"

#include "MidiLoopbackEndpointManager.h"
#include "MidiLoopbackEndpointCreationConfig.h"
#include "MidiLoopbackEndpointRemovalConfig.h"

#include "MidiVirtualPatchBayManager.h"
#include "MidiVirtualPatchBayDestinationDefinition.h"
#include "MidiVirtualPatchBayRouteCreationConfig.h"
#include "MidiVirtualPatchBayRouteDefinition.h"
#include "MidiVirtualPatchBayRouteRemovalConfig.h"
#include "MidiVirtualPatchBayRouteUpdateConfig.h"
#include "MidiVirtualPatchBaySourceDefinition.h"

#include "MidiStreamConfigRequestReceivedEventArgs.h"
#include "MidiVirtualDevice.h"
#include "MidiVirtualDeviceCreationConfig.h"
#include "MidiVirtualDeviceManager.h"

#include "midi_network_defs.h"
#include "MidiNetworkClientMatchCriteria.h"
#include "MidiNetworkHostCreationConfig.h"
#include "MidiNetworkHostRemovalConfig.h"
#include "MidiNetworkClientEndpointCreationConfig.h"
#include "MidiNetworkClientEndpointRemovalConfig.h"
#include "MidiNetworkEndpointManager.h"

#include "MidiNetworkAdvertisedHost.h"


#include "MidiNetworkAdvertisedHostAddedEventArgs.h"
#include "MidiNetworkAdvertisedHostUpdatedEventArgs.h"
#include "MidiNetworkAdvertisedHostRemovedEventArgs.h"
#include "MidiNetworkAdvertisedHostWatcher.h"



#include "MidiDiagnostics.h"
#include "MidiServicePingResponseSummary.h"

#include "MidiServiceSessionInfo.h"
#include "MidiReporting.h"

#include "MidiSystemExclusiveMessageHelper.h"
#include "MidiSystemExclusiveSender.h"

#include "MidiServiceConfig.h"

#include "MidiEndpointDevicePropertyHelper.h"

// Initialization-specific

#include <combaseapi.h>
//#include <comdef.h>
#include <ctxtcall.h>
#include <hstring.h>
#include <roapi.h>
#include <activationregistration.h>
#include <activation.h>
#include <Appmodel.h>
#include <RoMetadataApi.h>
#include <RoMetadata.h>
#include "../undocked-reg-free-winrt/detours/detours.h"

//#include <wrl/module.h>
//#include <wrl/event.h>

#include "Initialization/MidiAppSdkManifest.h"
#include "Initialization/MidiAppSdkRuntimeComponent.h"
#include "Initialization/MidiAppSdkRuntimeComponentCatalog.h"
#include "Initialization/MidiAppSdkRuntimeTypeResolution.h"
#include "Initialization/MidiRoDetours.h"

#include <shared_mutex>
#include "ComSingleton.h"
#include "Initialization/MidiClientInitializer.h"
#include "MidiClientInitializerSingleton.h"
#include "MidiClientInitializerFactory.h"

#include "MidiRuntimeVersion.h"
#include "MidiRuntimeRelease.h"
#include "MidiRuntimeUpdateUtility.h"
#include "MidiRuntimeInformation.h"

#include "MidiClockDestination.h"
#include "MidiClockGenerator.h"


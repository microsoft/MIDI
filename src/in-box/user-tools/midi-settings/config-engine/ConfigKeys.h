// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
//
// UNSUPPORTED - DO NOT COPY THIS APPROACH. This code reads and writes the Windows MIDI Services
// configuration file directly. Microsoft does not support direct manipulation of that file by
// anything other than the in-box MIDI tools. Its format, its name, its location and the registry
// value that selects it are implementation details and can change in any release without notice,
// so this code can corrupt a configuration or stop working at any time. It exists for development
// and diagnostics inside the Windows MIDI Services repository only. Applications and third-party
// tools must use the Windows MIDI Services API (Windows.Devices.Midi2.ServiceConfig) instead.
// ============================================================================

#pragma once

// The configuration file's keys this engine reads and writes. They mirror json_defs.h and the
// transports' own json defs headers, which can't be included here because they pull in the
// service's headers. Keep the two in step.

#include <string_view>

namespace midisettings::configengine::keys
{
    inline constexpr std::wstring_view Comment{ L"_comment" };
    inline constexpr std::wstring_view Header{ L"header" };
    inline constexpr std::wstring_view ConfigName{ L"configName" };
    inline constexpr std::wstring_view TransportPluginSettings{ L"endpointTransportPluginSettings" };

    inline constexpr std::wstring_view Create{ L"create" };
    inline constexpr std::wstring_view Update{ L"update" };
    inline constexpr std::wstring_view UpdateEntries{ L"updateEntries" };

    // endpoint customizations
    inline constexpr std::wstring_view Match{ L"match" };
    inline constexpr std::wstring_view CustomProperties{ L"customProperties" };
    inline constexpr std::wstring_view Name{ L"name" };
    inline constexpr std::wstring_view Image{ L"image" };
    inline constexpr std::wstring_view MatchEndpointDeviceId{ L"endpointDeviceId" };
    inline constexpr std::wstring_view MatchDeviceInstanceId{ L"endpointDeviceInstanceId" };
    inline constexpr std::wstring_view MatchTransportSuppliedName{ L"transportSuppliedEndpointName" };

    // loopbacks
    inline constexpr std::wstring_view LoopbackEndpointA{ L"endpointA" };
    inline constexpr std::wstring_view LoopbackEndpointB{ L"endpointB" };
    inline constexpr std::wstring_view LoopbackEndpoint{ L"endpoint" };
    inline constexpr std::wstring_view UniqueIdentifier{ L"uniqueIdentifier" };

    // Network MIDI 2.0 and RTP-MIDI
    inline constexpr std::wstring_view Hosts{ L"hosts" };
    inline constexpr std::wstring_view Clients{ L"clients" };
    inline constexpr std::wstring_view TransportSettings{ L"transportSettings" };
    inline constexpr std::wstring_view ServiceInstanceName{ L"serviceInstanceName" };
    inline constexpr std::wstring_view ProductInstanceId{ L"productInstanceId" };
    inline constexpr std::wstring_view NetworkAdapterId{ L"networkAdapterId" };
    inline constexpr std::wstring_view NetworkAdapterName{ L"networkAdapterName" };
    inline constexpr std::wstring_view NetworkAdapterPhysicalAddress{ L"networkAdapterPhysicalAddress" };
    inline constexpr std::wstring_view AllowNetworkAdapterFallback{ L"allowNetworkAdapterFallback" };
    inline constexpr std::wstring_view NetworkMatchId{ L"id" };
    inline constexpr std::wstring_view NetworkMatchServiceInstance{ L"serviceInstance" };
    inline constexpr std::wstring_view NetworkMatchDirectHost{ L"directHostNameOrIP" };
    inline constexpr std::wstring_view NetworkMatchUmpEndpointName{ L"umpEndpointName" };
    inline constexpr std::wstring_view RtpRemoteAddress{ L"remoteAddress" };
    inline constexpr std::wstring_view RtpRemotePort{ L"remotePort" };
    inline constexpr std::wstring_view RtpCustomEndpointName{ L"customEndpointName" };

    // Bluetooth
    inline constexpr std::wstring_view BluetoothDevices{ L"devices" };
    inline constexpr std::wstring_view BluetoothDeviceId{ L"deviceId" };

    // synthesizer
    inline constexpr std::wstring_view SynthMode{ L"synthMode" };
}

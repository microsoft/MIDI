// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "NetworkMidiTestClient.h"

namespace NetworkMidiTest
{
    // Environment overrides. These take priority over discovery, because mDNS is not dependable
    // inside a container and CI needs a deterministic target.
    //
    //   MIDI_NET2UDP_TEST_HOST   host name or IP of the host to test against
    //   MIDI_NET2UDP_TEST_PORT   UDP port of that host
    //
    // Setting both skips discovery entirely.
    constexpr wchar_t EnvironmentVariableHost[]{ L"MIDI_NET2UDP_TEST_HOST" };
    constexpr wchar_t EnvironmentVariablePort[]{ L"MIDI_NET2UDP_TEST_PORT" };

    struct HostLocatorResult
    {
        bool Found{ false };
        HostEndpointAddress Address{ };

        // Everything the locator saw, so a failing run says why it found nothing
        std::wstring Diagnostics{ };
        size_t TotalAdvertisedHostsSeen{ 0 };
    };

    // Finds a Network MIDI 2.0 host belonging to this machine. Never prompts, never blocks
    // indefinitely.
    HostLocatorResult LocateLocalServiceHost(_In_ std::chrono::milliseconds const discoveryTimeout);

    std::wstring GetLocalComputerName();
}

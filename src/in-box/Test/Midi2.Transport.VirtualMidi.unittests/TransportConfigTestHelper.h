// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

// Pushes hand-rolled configuration json into the running service the same way the SDK does.
//
// The SDK is the canonical path for these workloads and cleans everything it sends, so it can
// never produce the payloads these tests need. This is the only way to reach a transport's
// configuration manager with the untrusted input a hand-authored config file would supply.

#include <string>
#include <optional>

namespace TransportConfigTest
{
    struct ServiceConfigResult
    {
        bool CallSucceeded{ false };        // the RPC completed
        bool ReportedSuccess{ false };      // the transport reported success in its response
        std::wstring ResponseJson{ };
        std::wstring Message{ };

        bool IsSuccess() const { return CallSucceeded && ReportedSuccess; }
    };

    // Sends the transport's own object, wrapped in the endpointTransportPluginSettings and
    // transport GUID layers the service's RPC entry point re-parses and routes on.
    ServiceConfigResult SendTransportConfig(
        _In_ GUID const& transportId,
        _In_ std::wstring const& transportIdString,
        _In_ std::wstring const& transportObjectJson);

    // Sends a payload verbatim, with none of the wrapping above.
    ServiceConfigResult SendRawServiceConfig(
        _In_ GUID const& transportId,
        _In_ std::wstring const& rawPayload);

    // True if the transport answers at all. Tests skip themselves rather than fail when it is
    // not present, so a machine without the service produces no wall of red.
    bool IsTransportAvailable(
        _In_ GUID const& transportId,
        _In_ std::wstring const& transportIdString);

    std::wstring EscapeJsonString(_In_ std::wstring const& value);

    // A fresh association id / GUID string, for tests which need a unique one per run.
    std::wstring MakeGuidString();

    // Hex-digits-only GUID, which is the shape the SDK generates for unique ids.
    std::wstring MakeUniqueIdString();
}

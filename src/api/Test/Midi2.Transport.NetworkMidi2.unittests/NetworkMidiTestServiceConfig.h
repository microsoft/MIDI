// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

// Pushes configuration into the running service the same way the SDK does, so that tests can
// create and remove "midisrv as a client" connections without an external device.
//
// Callers pass just the transport's own object, with "create" / "transportSettings" /
// "transportCommand" at the top level. The endpointTransportPluginSettings and transport GUID
// wrappers are added here, because the service's RPC entry point re-parses the payload and
// routes on that GUID even though the configuration manager was already initialized with it.

#include <string>
#include <optional>

namespace NetworkMidiTest
{
    struct ServiceConfigResult
    {
        bool CallSucceeded{ false };        // the RPC completed
        bool ReportedSuccess{ false };      // the transport reported success in its response
        std::wstring ResponseJson{ };
        std::wstring Message{ };

        bool IsSuccess() const { return CallSucceeded && ReportedSuccess; }
    };


    // True if the service is reachable at all. Tests skip themselves rather than fail when it
    // is not, so a machine without the service running does not produce a wall of red.
    bool IsServiceAvailable();

    // Sends an already-built JSON payload to the network transport's configuration manager.
    ServiceConfigResult SendNetworkTransportConfig(_In_ std::wstring const& configJson);

    // Creates a client which connects directly to the given address and port. The entry
    // identifier is the GUID string the service uses to track the connection, and is what
    // RemoveClient needs later.
    ServiceConfigResult CreateDirectClient(
        _In_ std::wstring const& entryIdentifier,
        _In_ std::wstring const& hostNameOrAddress,
        _In_ uint16_t const port,
        _In_ bool const createMidi1Ports = false);

    // Disconnects and removes a client created above. Safe to call for an identifier which is
    // no longer present.
    ServiceConfigResult DisconnectClient(_In_ std::wstring const& entryIdentifier);

    // Returns the enumerateClients response, for tests which need to see what the service
    // believes is connected.
    ServiceConfigResult EnumerateClients();

    // A direct client is only attempted when the endpoint creator wakes, which defaults to
    // every 20 seconds. Tests shorten it so a connection attempt is prompt.
    ServiceConfigResult SetDirectConnectionScanInterval(_In_ uint32_t const milliseconds);

    // A fresh GUID string in the "{...}" form the configuration uses.
    std::wstring MakeEntryIdentifier();
}

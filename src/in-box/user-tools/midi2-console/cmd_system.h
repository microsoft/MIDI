// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midi2console
{
    struct ServiceStatusOptions
    {
        bool Verbose{ false };
    };

    struct ServicePingOptions
    {
        int Count{ 20 };
        int Timeout{ 10000 };
        bool Verbose{ false };
    };

    struct WatchEndpointsOptions
    {
        bool IncludeDiagnosticLoopback{ false };
        bool Verbose{ false };
    };

    struct WatchPortsOptions
    {
        bool Verbose{ false };
    };

    int RunTimeCommand();
    int RunServiceStatusCommand(_In_ ServiceStatusOptions const& options);
    int RunServicePingCommand(_In_ ServicePingOptions const& options);
    int RunWatchEndpointsCommand(_In_ WatchEndpointsOptions const& options);
    int RunWatchPortsCommand(_In_ WatchPortsOptions const& options);
}

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
    struct NetworkListOptions
    {
        bool Verbose{ false };
    };

    int RunNetworkHostsCommand(_In_ NetworkListOptions const& options);
    int RunNetworkClientsCommand(_In_ NetworkListOptions const& options);
    int RunNetworkBrowseCommand(_In_ NetworkListOptions const& options);
    int RunNetworkPendingCommand();
    int RunNetworkStatusCommand(_In_ NetworkListOptions const& options);
}

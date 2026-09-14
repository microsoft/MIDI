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
    struct ForwardOptions
    {
        std::string SourceEndpointDeviceId;
        std::string DestinationEndpointDeviceId;

        // Group numbers as the customer sees them, 1 to 16. Zero means "not supplied", which
        // is what sends the command to the picker.
        int SourceGroupNumber{ 0 };
        int DestinationGroupNumber{ 0 };
    };

    int RunForwardCommand(_In_ ForwardOptions const& options);
}

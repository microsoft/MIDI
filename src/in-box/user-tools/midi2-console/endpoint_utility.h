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
    std::string GetEndpointIcon(_In_ midi2enum::MidiEndpointDeviceInformation const& device);
    std::string GetEndpointIcon(_In_ midi2enum::MidiEndpointDevicePurpose purpose);

    std::string GetEndpointNameFromEndpointDeviceId(_In_ std::string const& endpointDeviceId);

    // Single place that decides which filter flags a listing uses, so enumerate, watch and the
    // picker all agree on what "an endpoint" means.
    midi2enum::MidiEndpointDeviceInformationFilters BuildEndpointFilters(
        _In_ bool includeDiagnosticLoopback,
        _In_ bool includeAll);

    collections::IVectorView<midi2enum::MidiEndpointDeviceInformation> EnumerateEndpoints(
        _In_ midi2enum::MidiEndpointDeviceInformationFilters filters);

    struct FriendlyTimeUnit
    {
        double Value{ 0.0 };
        std::string UnitLabel;
    };

    FriendlyTimeUnit ConvertTicksToFriendlyTimeUnit(_In_ uint64_t ticks);
}

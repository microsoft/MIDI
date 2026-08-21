// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midiapp
{
    // Resolves the manufacturer supplied endpoint image to a full path, or returns empty when
    // the stored name is not a plain file inside the shared assets folder.
    winrt::hstring ResolveEndpointImagePath(winrt::hstring const& imageFileName) noexcept;

    // Name of the function block or group terminal block covering this group, if any.
    winrt::hstring DescribeGroup(
        winrt::Windows::Devices::Midi2::Enumeration::MidiEndpointDeviceInformation const& endpoint,
        uint8_t groupIndex) noexcept;

    // Union of every group covered by a declared function block or group terminal block.
    // A device that declares nothing comes back with all sixteen set, so it stays usable.
    std::array<bool, 16> DeclaredGroups(
        winrt::Windows::Devices::Midi2::Enumeration::MidiEndpointDeviceInformation const& endpoint) noexcept;

    // Endpoints sorted by display name, the order every tool presents them in.
    std::vector<winrt::Windows::Devices::Midi2::Enumeration::MidiEndpointDeviceInformation>
        SortedEndpoints(
            winrt::Windows::Devices::Midi2::Enumeration::MidiEndpointDeviceWatcher const& watcher) noexcept;

    // Case insensitive endpoint id comparison. Ids differ in case between sources.
    bool EndpointIdsMatch(winrt::hstring const& left, winrt::hstring const& right) noexcept;
}

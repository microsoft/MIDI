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

    // Copies a chosen image into the shared endpoint assets folder and hands back the bare file
    // name to store. Only that folder is ever read from, so a picked file has to become a copy
    // there first. An identical copy is reused rather than duplicated, and a different file
    // which wants a taken name is given a new one rather than overwriting someone else's image.
    winrt::hstring ImportEndpointImage(winrt::hstring const& sourcePath) noexcept;

    // Name of the function block or group terminal block covering this group, if any.
    winrt::hstring DescribeGroup(
        winrt::Windows::Devices::Midi2::Enumeration::MidiEndpointDeviceInformation const& endpoint,
        uint8_t groupIndex) noexcept;

    // Union of every group covered by a declared function block or group terminal block.
    // A device that declares nothing comes back with all sixteen set, so it stays usable.
    std::array<bool, 16> DeclaredGroups(
        winrt::Windows::Devices::Midi2::Enumeration::MidiEndpointDeviceInformation const& endpoint) noexcept;

    // The same, split by which way the messages go. Block input and block output are named from
    // the DEVICE's point of view, so a block the device takes input on is a destination for us,
    // and one it sends output from is a source.
    struct GroupDirections
    {
        std::array<bool, 16> Sources{};
        std::array<bool, 16> Destinations{};

        int32_t SourceCount() const noexcept;
        int32_t DestinationCount() const noexcept;
    };

    GroupDirections DeclaredGroupDirections(
        winrt::Windows::Devices::Midi2::Enumeration::MidiEndpointDeviceInformation const& endpoint) noexcept;

    // The picture to show for an endpoint: the customer's own if they set one, otherwise the
    // transport's default, otherwise the plain default. The installer puts one default per
    // transport in the shared assets folder, named after the transport code.
    winrt::hstring ResolveEndpointImageOrDefault(
        winrt::hstring const& customImagePath,
        std::wstring const& transportCode) noexcept;

    // Endpoints sorted by display name, the order every tool presents them in.
    std::vector<winrt::Windows::Devices::Midi2::Enumeration::MidiEndpointDeviceInformation>
        SortedEndpoints(
            winrt::Windows::Devices::Midi2::Enumeration::MidiEndpointDeviceWatcher const& watcher) noexcept;

    // Case insensitive endpoint id comparison. Ids differ in case between sources.
    bool EndpointIdsMatch(winrt::hstring const& left, winrt::hstring const& right) noexcept;
}

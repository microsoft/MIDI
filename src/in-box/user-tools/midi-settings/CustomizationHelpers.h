// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midiapp::customizations
{
    // The customization the service currently holds for a live endpoint, or null when there is
    // none. Transports which cannot report their customizations simply return nothing.
    midi2config::MidiServiceEndpointCustomization FindCustomizationForEndpoint(
        _In_ winrt::guid const& transportId,
        _In_ winrt::hstring const& endpointDeviceId) noexcept;

    // Anything stored beyond the name, description and image. Clearing those three only means the
    // entry can be deleted when there is nothing else in it.
    bool HasNonDisplayContent(
        _In_ midi2config::MidiServiceEndpointCustomization const& customization) noexcept;

    // Provenance is what makes an entry recognizable once its endpoint id no longer matches
    // anything. The device facts come from whichever endpoint the entry belongs to now; the
    // original creation date and any measurement record are carried forward.
    midi2config::MidiServiceEndpointCustomizationProvenance BuildProvenance(
        _In_ winrt::hstring const& endpointDeviceId,
        _In_ midi2config::MidiServiceEndpointCustomizationProvenance const& existing) noexcept;

    // A short description of what an entry holds, for a customer deciding whether they still want
    // it. Everything counts, not just the visible fields: a measured latency is the hardest of
    // these to reproduce.
    winrt::hstring DescribeContent(
        _In_ midi2config::MidiServiceEndpointCustomization const& customization) noexcept;

    // Copies every stored property onto a configuration aimed at a different endpoint.
    void CopyInto(
        _In_ midi2config::MidiServiceEndpointCustomization const& source,
        _In_ midi2config::MidiServiceEndpointCustomizationConfig const& destination) noexcept;
}

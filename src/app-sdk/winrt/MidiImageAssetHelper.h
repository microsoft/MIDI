// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Utilities.Metadata.MidiImageAssetHelper.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Utilities::Metadata::implementation
{
    struct MidiImageAssetHelper
    {
        MidiImageAssetHelper() = default;

        static winrt::hstring GetSmallImageFullPathForTransport(
            _In_ midi2::Reporting::MidiServiceTransportPluginInfo const& transportPluginInfo) noexcept;

        static winrt::hstring GetDefaultSmallImageFullPathForTransport(
            _In_ midi2::Reporting::MidiServiceTransportPluginInfo const& transportPluginInfo) noexcept;

        static winrt::hstring GetSmallImageFullPathForEndpoint(
            _In_ midi2::MidiEndpointDeviceInformation const& endpointDeviceInformation) noexcept;

        static winrt::hstring GetDefaultSmallImageFullPathForEndpoint(
            _In_ midi2::MidiEndpointDeviceInformation const& endpointDeviceInformation) noexcept;
    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Utilities::Metadata::factory_implementation
{
    struct MidiImageAssetHelper : MidiImageAssetHelperT<MidiImageAssetHelper, implementation::MidiImageAssetHelper>
    {
    };
}

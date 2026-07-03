// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "MidiEndpointDeviceIdHelper.g.h"

namespace winrt::Windows::Devices::Midi2::Enumeration::implementation
{
    struct MidiEndpointDeviceIdHelper
    {
        //MidiEndpointDeviceIdHelper() = default;

        static winrt::hstring GetShortIdFromFullId(_In_ winrt::hstring const& fullEndpointDeviceId) noexcept;
        static winrt::hstring GetFullIdFromShortId(_In_ winrt::hstring const& shortEndpointDeviceId) noexcept;
        static bool IsPossibleWindowsMidiServicesEndpointDeviceId(_In_ winrt::hstring const& fullEndpointDeviceId) noexcept;

        static winrt::hstring NormalizeFullId(_In_ winrt::hstring const& fullEndpointDeviceId) noexcept;
    };
}
namespace winrt::Windows::Devices::Midi2::Enumeration::factory_implementation
{
    struct MidiEndpointDeviceIdHelper : MidiEndpointDeviceIdHelperT<MidiEndpointDeviceIdHelper, implementation::MidiEndpointDeviceIdHelper, winrt::static_lifetime>
    {
    };
}

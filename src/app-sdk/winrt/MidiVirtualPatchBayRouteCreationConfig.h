// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "VirtualPatchBay.MidiVirtualPatchBayRouteCreationConfig.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::VirtualPatchBay::implementation
{
    struct MidiVirtualPatchBayRouteCreationConfig : MidiVirtualPatchBayRouteCreationConfigT<MidiVirtualPatchBayRouteCreationConfig>
    {
        MidiVirtualPatchBayRouteCreationConfig() = default;
        MidiVirtualPatchBayRouteCreationConfig(_In_ vpb::MidiVirtualPatchBayRouteDefinition const& definition) noexcept;

        winrt::guid TransportId() { return m_transportId; }

        bool IsFromCurrentConfigFile() { return m_isFromCurrentConfigFile; }
        json::JsonObject GetConfigJson();


        vpb::MidiVirtualPatchBayRouteDefinition Definition() const noexcept{ return m_routeDefinition; }
        void Definition(_In_ vpb::MidiVirtualPatchBayRouteDefinition const& value) noexcept { m_routeDefinition = value; }

    private:
        winrt::guid m_routeId{};
        winrt::guid m_transportId{};
        bool m_isFromCurrentConfigFile{ false };

        vpb::MidiVirtualPatchBayRouteDefinition m_routeDefinition{ nullptr };

    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::VirtualPatchBay::factory_implementation
{
    struct MidiVirtualPatchBayRouteCreationConfig : MidiVirtualPatchBayRouteCreationConfigT<MidiVirtualPatchBayRouteCreationConfig, implementation::MidiVirtualPatchBayRouteCreationConfig>
    {
    };
}

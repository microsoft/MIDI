// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "VirtualPatchBay.MidiVirtualPatchBayRouteDefinition.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::VirtualPatchBay::implementation
{
    struct MidiVirtualPatchBayRouteDefinition : MidiVirtualPatchBayRouteDefinitionT<MidiVirtualPatchBayRouteDefinition>
    {
        MidiVirtualPatchBayRouteDefinition() = default;

        MidiVirtualPatchBayRouteDefinition(_In_ winrt::hstring const& name) { m_name = name; }

        MidiVirtualPatchBayRouteDefinition(
            _In_ winrt::hstring const& name, 
            _In_ winrt::hstring const& description) { Name(name); Description(description); }


        winrt::guid RouteId() { return m_routeId; }

        winrt::hstring Name() const noexcept { return m_name; }
        void Name(_In_ winrt::hstring const& value) noexcept { m_name = value; }

        winrt::hstring Description() const noexcept { return m_description; }
        void Description(_In_ winrt::hstring const& value) noexcept { m_description = value; }

        bool IsEnabled() const noexcept { return m_isEnabled; }
        void IsEnabled(_In_ bool const value) noexcept { m_isEnabled = value; }

        collections::IVector<vpb::MidiVirtualPatchBaySourceDefinition> Sources() noexcept { return m_sources; }
        collections::IVector<vpb::MidiVirtualPatchBayDestinationDefinition> Destinations() noexcept { return m_destinations; }

        json::JsonObject GetConfigJson() const noexcept;
        static vpb::MidiVirtualPatchBayRouteDefinition CreateFromConfigJson(_In_ winrt::hstring const& jsonSection) noexcept;


    private:
        winrt::guid m_routeId { foundation::GuidHelper::CreateNewGuid() };

        bool m_isEnabled{ true };

        winrt::hstring m_name{};
        winrt::hstring m_description{};


        collections::IVector<vpb::MidiVirtualPatchBaySourceDefinition> m_sources { 
            winrt::multi_threaded_vector<vpb::MidiVirtualPatchBaySourceDefinition>() };

        collections::IVector<vpb::MidiVirtualPatchBayDestinationDefinition> m_destinations{ 
            winrt::multi_threaded_vector<vpb::MidiVirtualPatchBayDestinationDefinition>() };

    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::VirtualPatchBay::factory_implementation
{
    struct MidiVirtualPatchBayRouteDefinition : MidiVirtualPatchBayRouteDefinitionT<MidiVirtualPatchBayRouteDefinition, implementation::MidiVirtualPatchBayRouteDefinition>
    {
    };
}

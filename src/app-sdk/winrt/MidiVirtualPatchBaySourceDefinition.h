// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "VirtualPatchBay.MidiVirtualPatchBaySourceDefinition.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::VirtualPatchBay::implementation
{
    struct MidiVirtualPatchBaySourceDefinition : MidiVirtualPatchBaySourceDefinitionT<MidiVirtualPatchBaySourceDefinition>
    {
        MidiVirtualPatchBaySourceDefinition() = default;

        MidiVirtualPatchBaySourceDefinition(_In_ svc::MidiServiceConfigEndpointMatchCriteria const& matchCriteria) { m_matchCriteria = matchCriteria; }

        bool IsEnabled() const noexcept { return m_isEnabled; }
        void IsEnabled(_In_ bool value) noexcept { m_isEnabled = value; }

        svc::MidiServiceConfigEndpointMatchCriteria EndpointMatchCriteria() const noexcept { return m_matchCriteria; }
        void EndpointMatchCriteria(_In_ svc::MidiServiceConfigEndpointMatchCriteria const& value) noexcept { m_matchCriteria = value; }

        static vpb::MidiVirtualPatchBaySourceDefinition CreateFromConfigJson(_In_ winrt::hstring const& jsonSection) noexcept;
        json::JsonObject GetConfigJson() const noexcept;

    private:
        bool m_isEnabled{ true };
               
        svc::MidiServiceConfigEndpointMatchCriteria m_matchCriteria{ nullptr };
    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::VirtualPatchBay::factory_implementation
{
    struct MidiVirtualPatchBaySourceDefinition : MidiVirtualPatchBaySourceDefinitionT<MidiVirtualPatchBaySourceDefinition, implementation::MidiVirtualPatchBaySourceDefinition>
    {
    };
}

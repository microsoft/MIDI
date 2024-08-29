// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "VirtualPatchBay.MidiVirtualPatchBayDestinationDefinition.g.h"


namespace winrt::Microsoft::Windows::Devices::Midi2::VirtualPatchBay::implementation
{
    struct MidiVirtualPatchBayDestinationDefinition : MidiVirtualPatchBayDestinationDefinitionT<MidiVirtualPatchBayDestinationDefinition>
    {
        MidiVirtualPatchBayDestinationDefinition() = default;

        bool IsEnabled() { return m_isEnabled; }
        void IsEnabled(_In_ bool value) { m_isEnabled = value; }

        winrt::hstring EndpointDeviceId() { return m_endpointDeviceId; }
        void EndpointDeviceId (_In_ winrt::hstring const& value) { m_endpointDeviceId = value; }

        collections::IMap<midi2::MidiGroup, midi2::MidiGroup> GroupTransformMap();

    private:
        bool m_isEnabled{ true };
        winrt::hstring m_endpointDeviceId{};
    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::VirtualPatchBay::factory_implementation
{
    struct MidiVirtualPatchBayDestinationDefinition : MidiVirtualPatchBayDestinationDefinitionT<MidiVirtualPatchBayDestinationDefinition, implementation::MidiVirtualPatchBayDestinationDefinition>
    {
    };
}

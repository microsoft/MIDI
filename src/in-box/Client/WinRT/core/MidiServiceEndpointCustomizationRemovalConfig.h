// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "ServiceConfig.MidiServiceEndpointCustomizationRemovalConfig.g.h"

namespace winrt::Windows::Devices::Midi2::ServiceConfig::implementation
{
    struct MidiServiceEndpointCustomizationRemovalConfig : MidiServiceEndpointCustomizationRemovalConfigT<MidiServiceEndpointCustomizationRemovalConfig>
    {
        MidiServiceEndpointCustomizationRemovalConfig() = default;
        MidiServiceEndpointCustomizationRemovalConfig(_In_ winrt::guid const& transportId);
        MidiServiceEndpointCustomizationRemovalConfig(
            _In_ winrt::guid const& transportId,
            _In_ svc::MidiServiceConfigEndpointMatchCriteria const& matchCriteria);

        winrt::guid TransportId() const noexcept { return m_transportId; }

        json::JsonObject ConfigJson() const noexcept;

        svc::MidiServiceConfigEndpointMatchCriteria MatchCriteria() const noexcept { return m_matchCriteria; }
        void MatchCriteria(_In_ svc::MidiServiceConfigEndpointMatchCriteria const& value) noexcept { m_matchCriteria = value; }

    private:
        winrt::guid m_transportId{};
        svc::MidiServiceConfigEndpointMatchCriteria m_matchCriteria{ nullptr };
    };
}

namespace winrt::Windows::Devices::Midi2::ServiceConfig::factory_implementation
{
    struct MidiServiceEndpointCustomizationRemovalConfig : MidiServiceEndpointCustomizationRemovalConfigT<MidiServiceEndpointCustomizationRemovalConfig, implementation::MidiServiceEndpointCustomizationRemovalConfig>
    {
    };
}

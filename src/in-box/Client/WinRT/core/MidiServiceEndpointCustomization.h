// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "ServiceConfig.MidiServiceEndpointCustomization.g.h"

#include "MidiEndpointCustomProperties.h"
#include "MidiEndpointCustomizationProcessor.h"

namespace winrt::Windows::Devices::Midi2::ServiceConfig::implementation
{
    struct MidiServiceEndpointCustomization : MidiServiceEndpointCustomizationT<MidiServiceEndpointCustomization>
    {
        MidiServiceEndpointCustomization() = default;

        winrt::guid TransportId() const noexcept { return m_transportId; }

        midi2::ServiceConfig::MidiServiceConfigEndpointMatchCriteria MatchCriteria() const noexcept { return m_matchCriteria; }
        midi2::ServiceConfig::MidiServiceEndpointCustomizationProvenance Provenance() const noexcept { return m_provenance; }

        winrt::hstring Name() const noexcept { return m_properties->Name; }
        winrt::hstring Description() const noexcept { return m_properties->Description; }
        winrt::hstring ImageFileName() const noexcept { return m_properties->Image; }

        bool RequiresNoteOffTranslation() const noexcept { return m_properties->RequiresNoteOffTranslation; }
        bool SupportsMidiPolyphonicExpression() const noexcept { return m_properties->SupportsMidiPolyphonicExpression; }
        uint16_t RecommendedControlChangeIntervalMilliseconds() const noexcept { return m_properties->RecommendedControlChangeIntervalMilliseconds; }

        int64_t OutgoingLatencyTicks() const noexcept { return m_properties->OutgoingLatencyTicks; }
        bool UseCustomOutgoingLatency() const noexcept { return m_properties->UseCustomOutgoingLatency; }

        midi2enum::Midi1PortNamingApproach Midi1PortNamingApproach() const noexcept;

        collections::IMapView<uint8_t, winrt::hstring> Midi1SourcePortCustomNames() const noexcept { return m_sourceNames.GetView(); }
        collections::IMapView<uint8_t, winrt::hstring> Midi1DestinationPortCustomNames() const noexcept { return m_destinationNames.GetView(); }

        winrt::hstring ResolvedEndpointDeviceId() const noexcept { return m_resolvedEndpointDeviceId; }
        bool IsOrphaned() const noexcept { return m_resolvedEndpointDeviceId.empty(); }
        bool HasUserContent() const noexcept { return m_hasUserContent; }

        void InternalInitializeFromJson(
            _In_ winrt::guid const& transportId,
            _In_ json::JsonObject const& entryObject) noexcept;

    private:
        winrt::guid m_transportId{};

        midi2::ServiceConfig::MidiServiceConfigEndpointMatchCriteria m_matchCriteria{};
        midi2::ServiceConfig::MidiServiceEndpointCustomizationProvenance m_provenance{};

        std::shared_ptr<WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomProperties> m_properties{
            std::make_shared<WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomProperties>() };

        collections::IMap<uint8_t, winrt::hstring> m_sourceNames{ winrt::single_threaded_map<uint8_t, winrt::hstring>() };
        collections::IMap<uint8_t, winrt::hstring> m_destinationNames{ winrt::single_threaded_map<uint8_t, winrt::hstring>() };

        winrt::hstring m_resolvedEndpointDeviceId{};
        bool m_hasUserContent{ false };
    };
}

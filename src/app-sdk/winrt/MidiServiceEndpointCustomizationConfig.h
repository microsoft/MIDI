// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "ServiceConfig.MidiServiceEndpointCustomizationConfig.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::implementation
{
    struct MidiServiceEndpointCustomizationConfig : MidiServiceEndpointCustomizationConfigT<MidiServiceEndpointCustomizationConfig>
    {
        MidiServiceEndpointCustomizationConfig() = default;

        MidiServiceEndpointCustomizationConfig(_In_ winrt::guid const& transportId) noexcept;

        MidiServiceEndpointCustomizationConfig(
            _In_ winrt::guid const& transportId, 
            _In_ winrt::hstring const& name, 
            _In_ winrt::hstring const& description) noexcept;

        MidiServiceEndpointCustomizationConfig(
            _In_ winrt::guid const& transportId, 
            _In_ winrt::hstring const& name, 
            _In_ winrt::hstring const& description, 
            _In_ winrt::hstring const& largeImageFileName, 
            _In_ winrt::hstring const& smallImageFileName) noexcept;

        MidiServiceEndpointCustomizationConfig(
            _In_ winrt::guid const& transportId, 
            _In_ winrt::hstring const& name, 
            _In_ winrt::hstring const& description, 
            _In_ winrt::hstring const& largeImageFileName, 
            _In_ winrt::hstring const& smallImageFileName, 
            _In_ bool const requiresNoteOffTranslation,
            _In_ bool const supportsMidiPolyphonicExpression,
            _In_ uint16_t const recommendedControlChangeIntervalMilliseconds) noexcept;

        hstring Name() const noexcept { return m_name ; }
        void Name(_In_ winrt::hstring const& value) noexcept { m_name = internal::TrimmedHStringCopy(value); }

        hstring Description() const noexcept { return m_description; }
        void Description(_In_ winrt::hstring const& value) noexcept { m_description = internal::TrimmedHStringCopy(value); }

        hstring LargeImageFileName() const noexcept { return m_largeImageFileName; }
        void LargeImageFileName(_In_ winrt::hstring const& value) noexcept { m_largeImageFileName = internal::TrimmedHStringCopy(value); }

        hstring SmallImageFileName() const noexcept { return m_smallImageFileName; }
        void SmallImageFileName(_In_ winrt::hstring const& value) noexcept { m_smallImageFileName = internal::TrimmedHStringCopy(value); }
        
        bool RequiresNoteOffTranslation() const noexcept { return m_requiresNoteOffTranslation; }
        void RequiresNoteOffTranslation(_In_ bool const value) noexcept { m_requiresNoteOffTranslation = value; }

        bool SupportsMidiPolyphonicExpression() const noexcept { return m_supportsMidiPolyphonicExpression; }
        void SupportsMidiPolyphonicExpression(_In_ bool const value) noexcept { m_supportsMidiPolyphonicExpression = value; }

        uint16_t RecommendedControlChangeIntervalMilliseconds() const noexcept { return m_recommendedControlChangeIntervalMilliseconds; }
        void RecommendedControlChangeIntervalMilliseconds(_In_ uint16_t const value) noexcept { m_recommendedControlChangeIntervalMilliseconds = value; }

        winrt::guid TransportId() const noexcept { return m_transportId; }
        void TransportId(_In_ winrt::guid const& value) noexcept { m_transportId = value; }

        midi2::ServiceConfig::MidiServiceConfigEndpointMatchCriteria MatchCriteria() { return m_matchCriteria; }
        void MatchCriteria(_In_ midi2::ServiceConfig::MidiServiceConfigEndpointMatchCriteria const& value) { m_matchCriteria = value; }


        hstring GetConfigJson() const noexcept;

    private:
        winrt::hstring m_name{};
        winrt::hstring m_description{};
        winrt::hstring m_largeImageFileName{};
        winrt::hstring m_smallImageFileName{};
        bool m_requiresNoteOffTranslation{ false };
        bool m_supportsMidiPolyphonicExpression{ false };
        uint16_t m_recommendedControlChangeIntervalMilliseconds{ 0 };

        midi2::ServiceConfig::MidiServiceConfigEndpointMatchCriteria m_matchCriteria{};

        winrt::guid m_transportId{};

        collections::IMap<winrt::hstring, winrt::hstring> m_matchValues = winrt::single_threaded_map<winrt::hstring, winrt::hstring>();
    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::factory_implementation
{
    struct MidiServiceEndpointCustomizationConfig : MidiServiceEndpointCustomizationConfigT<MidiServiceEndpointCustomizationConfig, implementation::MidiServiceEndpointCustomizationConfig>
    {
    };
}

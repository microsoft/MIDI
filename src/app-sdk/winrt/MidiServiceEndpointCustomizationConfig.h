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
            _In_ winrt::hstring const& imageFileName) noexcept;

        MidiServiceEndpointCustomizationConfig(
            _In_ winrt::guid const& transportId, 
            _In_ winrt::hstring const& name, 
            _In_ winrt::hstring const& description, 
            _In_ winrt::hstring const& imageFileName, 
            _In_ bool const requiresNoteOffTranslation,
            _In_ bool const supportsMidiPolyphonicExpression,
            _In_ uint16_t const recommendedControlChangeIntervalMilliseconds) noexcept;

        winrt::hstring Name() const noexcept { return m_props->Name ; }
        void Name(_In_ winrt::hstring const& value) noexcept { m_props->Name = internal::TrimmedHStringCopy(value); }

        winrt::hstring Description() const noexcept { return m_props->Description; }
        void Description(_In_ winrt::hstring const& value) noexcept { m_props->Description = internal::TrimmedHStringCopy(value); }

        winrt::hstring ImageFileName() const noexcept { return m_props->Image; }
        void ImageFileName(_In_ winrt::hstring const& value) noexcept { m_props->Image = internal::TrimmedHStringCopy(value); }
        
        bool RequiresNoteOffTranslation() const noexcept { return m_props->RequiresNoteOffTranslation; }
        void RequiresNoteOffTranslation(_In_ bool const value) noexcept { m_props->RequiresNoteOffTranslation = value; }

        bool SupportsMidiPolyphonicExpression() const noexcept { return m_props->SupportsMidiPolyphonicExpression; }
        void SupportsMidiPolyphonicExpression(_In_ bool const value) noexcept { m_props->SupportsMidiPolyphonicExpression = value; }

        uint16_t RecommendedControlChangeIntervalMilliseconds() const noexcept { return m_props->RecommendedControlChangeIntervalMilliseconds; }
        void RecommendedControlChangeIntervalMilliseconds(_In_ uint16_t const value) noexcept { m_props->RecommendedControlChangeIntervalMilliseconds = value; }

        winrt::guid TransportId() const noexcept { return m_transportId; }
        void TransportId(_In_ winrt::guid const& value) noexcept { m_transportId = value; }

        midi2::ServiceConfig::MidiServiceConfigEndpointMatchCriteria MatchCriteria() { return m_matchCriteria; }
        void MatchCriteria(_In_ midi2::ServiceConfig::MidiServiceConfigEndpointMatchCriteria const& value) { m_matchCriteria = value; }


        hstring GetConfigJson() const noexcept;

    private:
        std::shared_ptr<MidiEndpointCustomProperties> m_props{ std::make_shared<MidiEndpointCustomProperties>() };
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

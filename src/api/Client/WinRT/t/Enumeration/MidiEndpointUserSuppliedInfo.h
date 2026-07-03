// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "MidiEndpointUserSuppliedInfo.g.h"


namespace winrt::Windows::Devices::Midi2::Enumeration::implementation
{
    struct MidiEndpointUserSuppliedInfo : MidiEndpointUserSuppliedInfoT<MidiEndpointUserSuppliedInfo>
    {
        MidiEndpointUserSuppliedInfo() = default;

        MidiEndpointUserSuppliedInfo(
            _In_ winrt::hstring const& name,
            _In_ winrt::hstring const& imageFileName,
            _In_ bool const requiresNoteOffTranslation,
            _In_ uint16_t const recommendedControlChangeAutomationIntervalMilliseconds,
            _In_ bool const supportsMidiPolyphonicExpression,
            _In_ uint64_t const customMidiOutgoingLatencyTicks,
            _In_ bool const useCustomMidiOutgoingLatencyTicksForScheduling
        )
        {
            m_name = name;
            m_imageFileName = imageFileName;
            m_requiresNoteOffTranslation = requiresNoteOffTranslation;
            m_recommendedControlChangeAutomationIntervalMilliseconds = recommendedControlChangeAutomationIntervalMilliseconds;
            m_supportsMidiPolyphonicExpression = supportsMidiPolyphonicExpression;
            m_customMidiOutgoingLatencyTicks = customMidiOutgoingLatencyTicks;
            m_useCustomMidiOutgoingLatencyTicksForScheduling = useCustomMidiOutgoingLatencyTicksForScheduling;
        }

        bool IsReadOnly() const noexcept { return m_isReadOnly;}
        
        winrt::hstring Name() const noexcept { return m_name; }
        void Name(_In_ winrt::hstring const& value) noexcept
        {
            if (IsReadOnly()) return;
            m_name = value;
        }

        winrt::hstring Description() const noexcept { return m_description; }
        void Description(_In_ winrt::hstring const& value) noexcept
        {
            if (IsReadOnly()) return;
            m_description = value;
        }

        winrt::hstring ImageFileName() const noexcept { return m_imageFileName; }
        void ImageFileName(_In_ winrt::hstring const& value) noexcept
        {
            if (IsReadOnly()) return;
            m_imageFileName = value;
        }

        bool RequiresNoteOffTranslation() const noexcept { return m_requiresNoteOffTranslation; }
        void RequiresNoteOffTranslation(_In_ bool const value) noexcept
        {
            if (IsReadOnly()) return;
            m_requiresNoteOffTranslation = value;
        }

        uint16_t RecommendedControlChangeAutomationIntervalMilliseconds() const noexcept { return m_recommendedControlChangeAutomationIntervalMilliseconds; }
        void RecommendedControlChangeAutomationIntervalMilliseconds(_In_ uint16_t value) noexcept
        {
            if (IsReadOnly()) return;
            m_recommendedControlChangeAutomationIntervalMilliseconds = value;
        }

        bool SupportsMidiPolyphonicExpression() const noexcept { return m_supportsMidiPolyphonicExpression; }
        void SupportsMidiPolyphonicExpression(_In_ bool const value) noexcept
        { 
            if (IsReadOnly()) return;
            m_supportsMidiPolyphonicExpression = value;
        }

        uint64_t CustomMidiOutgoingLatencyTicks() const noexcept { return m_customMidiOutgoingLatencyTicks; }
        void CustomMidiOutgoingLatencyTicks(_In_ uint64_t const value) noexcept
        {
            if (IsReadOnly()) return;
            m_customMidiOutgoingLatencyTicks = value;
        }

        bool UseCustomMidiOutgoingLatencyTicksForScheduling() const noexcept { return m_useCustomMidiOutgoingLatencyTicksForScheduling; }
        void UseCustomMidiOutgoingLatencyTicksForScheduling(_In_ bool const value) noexcept
        {
            if (IsReadOnly()) return;
            m_useCustomMidiOutgoingLatencyTicksForScheduling = value;
        }

        void InternalSetReadOnly() noexcept { m_isReadOnly = true; }


    private:
        bool m_isReadOnly{ false };
        winrt::hstring m_name{};
        winrt::hstring m_description{};
        winrt::hstring m_imageFileName{};
        bool m_requiresNoteOffTranslation{ false };
        uint16_t m_recommendedControlChangeAutomationIntervalMilliseconds{ 0 };
        bool m_supportsMidiPolyphonicExpression{ false };
        uint64_t m_customMidiOutgoingLatencyTicks{ 0 };
        bool m_useCustomMidiOutgoingLatencyTicksForScheduling{ false };

    };
}
namespace winrt::Windows::Devices::Midi2::Enumeration::factory_implementation
{
    struct MidiEndpointUserSuppliedInfo : MidiEndpointUserSuppliedInfoT<MidiEndpointUserSuppliedInfo, implementation::MidiEndpointUserSuppliedInfo>
    {};
}
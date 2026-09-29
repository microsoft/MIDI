// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Loopback.MidiLoopbackSavedEntry.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Loopback::implementation
{
    struct MidiLoopbackSavedEntry : MidiLoopbackSavedEntryT<MidiLoopbackSavedEntry>
    {
        MidiLoopbackSavedEntry() = default;

        winrt::guid AssociationId() const noexcept { return m_associationId; }

        loop::MidiLoopbackEndpointDefinition EndpointDefinitionA() const noexcept { return m_definitionA; }
        loop::MidiLoopbackEndpointDefinition EndpointDefinitionB() const noexcept { return m_definitionB; }

        bool IsMuted() const noexcept { return m_isMuted; }

        loop::MidiLoopbackFeedbackProtection FeedbackProtection() const noexcept { return m_feedbackProtection; }

        void InternalInitialize(
            _In_ winrt::guid const& associationId,
            _In_ loop::MidiLoopbackEndpointDefinition const& definitionA,
            _In_ loop::MidiLoopbackEndpointDefinition const& definitionB,
            _In_ bool const isMuted,
            _In_ loop::MidiLoopbackFeedbackProtection const feedbackProtection) noexcept
        {
            m_associationId = associationId;
            m_definitionA = definitionA;
            m_definitionB = definitionB;
            m_isMuted = isMuted;
            m_feedbackProtection = feedbackProtection;
        }

    private:
        winrt::guid m_associationId{};
        loop::MidiLoopbackEndpointDefinition m_definitionA{ nullptr };
        loop::MidiLoopbackEndpointDefinition m_definitionB{ nullptr };
        bool m_isMuted{ false };
        loop::MidiLoopbackFeedbackProtection m_feedbackProtection{ loop::MidiLoopbackFeedbackProtection::Mute };
    };
}

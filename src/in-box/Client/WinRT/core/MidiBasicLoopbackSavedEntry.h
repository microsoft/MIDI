// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.BasicLoopback.MidiBasicLoopbackSavedEntry.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::BasicLoopback::implementation
{
    struct MidiBasicLoopbackSavedEntry : MidiBasicLoopbackSavedEntryT<MidiBasicLoopbackSavedEntry>
    {
        MidiBasicLoopbackSavedEntry() = default;

        winrt::guid AssociationId() const noexcept { return m_associationId; }

        bloop::MidiBasicLoopbackEndpointDefinition EndpointDefinition() const noexcept { return m_definition; }

        bool IsMuted() const noexcept { return m_isMuted; }

        bloop::MidiBasicLoopbackFeedbackProtection FeedbackProtection() const noexcept { return m_feedbackProtection; }

        void InternalInitialize(
            _In_ winrt::guid const& associationId,
            _In_ bloop::MidiBasicLoopbackEndpointDefinition const& definition,
            _In_ bool const isMuted,
            _In_ bloop::MidiBasicLoopbackFeedbackProtection const feedbackProtection) noexcept
        {
            m_associationId = associationId;
            m_definition = definition;
            m_isMuted = isMuted;
            m_feedbackProtection = feedbackProtection;
        }

    private:
        winrt::guid m_associationId{};
        bloop::MidiBasicLoopbackEndpointDefinition m_definition{ nullptr };
        bool m_isMuted{ false };
        bloop::MidiBasicLoopbackFeedbackProtection m_feedbackProtection{ bloop::MidiBasicLoopbackFeedbackProtection::Mute };
    };
}

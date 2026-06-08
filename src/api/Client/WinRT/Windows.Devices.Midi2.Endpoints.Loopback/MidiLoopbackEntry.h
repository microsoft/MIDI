// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "MidiLoopbackEntry.g.h"

namespace winrt::Windows::Devices::Midi2::Endpoints::Loopback::implementation
{
    struct MidiLoopbackEntry : MidiLoopbackEntryT<MidiLoopbackEntry>
    {
        MidiLoopbackEntry() = default;

        winrt::guid AssociationId() const noexcept { return m_associationId; }
        loop::MidiLoopbackEndpointEntry EndpointA() const noexcept { return m_endpointA; }
        loop::MidiLoopbackEndpointEntry EndpointB() const noexcept { return m_endpointB; }
        bool IsMuted() const noexcept { return m_isMuted; }

        void InternalSetAssociationId(_In_ winrt::guid const& associationId) noexcept { m_associationId = associationId; }
        void InternalSetEndpointEntries(
            _In_ loop::MidiLoopbackEndpointEntry const& endpointA, 
            _In_ loop::MidiLoopbackEndpointEntry const& endpointB) noexcept
        { 
            m_endpointA = endpointA; 
            m_endpointB = endpointB; 
        }

        void InternalSetMuted(_In_ bool isMuted) noexcept { m_isMuted = isMuted; }

    private:        
        winrt::guid m_associationId;
        loop::MidiLoopbackEndpointEntry m_endpointA{ nullptr };
        loop::MidiLoopbackEndpointEntry m_endpointB{ nullptr };
        bool m_isMuted{ false };
    };
}

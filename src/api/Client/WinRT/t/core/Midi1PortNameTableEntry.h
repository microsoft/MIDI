// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Enumeration.Midi1PortNameTableEntry.g.h"

namespace winrt::Windows::Devices::Midi2::Enumeration::implementation
{
    struct Midi1PortNameTableEntry : Midi1PortNameTableEntryT<Midi1PortNameTableEntry>
    {
        Midi1PortNameTableEntry() = default;

        midi2::MidiGroup Group() const noexcept { return m_group; }
        midi2enum::Midi1PortFlow Flow() const noexcept { return m_flow; }
        winrt::hstring CustomName() const noexcept { return m_customName; }
        winrt::hstring LegacyCompatibleName() const noexcept { return m_legacyCompatibleName; }
        winrt::hstring NewStyleName() const noexcept { return m_newStyleName; }

        void InternalInitialize(
            _In_ midi2::MidiGroup const& group, 
            _In_ midi2enum::Midi1PortFlow const& flow, 
            _In_ winrt::hstring const& customName, 
            _In_ winrt::hstring const& legacyCompatibleName, 
            _In_ winrt::hstring const& newStyleName
        ) noexcept
        {
            m_group = group;
            m_flow = flow;
            m_customName = customName;
            m_legacyCompatibleName = legacyCompatibleName;
            m_newStyleName = newStyleName; 
        }

    private:
        midi2::MidiGroup m_group{ static_cast<uint8_t>(0) };
        midi2enum::Midi1PortFlow m_flow{};
        winrt::hstring m_customName{};
        winrt::hstring m_legacyCompatibleName{};
        winrt::hstring m_newStyleName{};

    };
}

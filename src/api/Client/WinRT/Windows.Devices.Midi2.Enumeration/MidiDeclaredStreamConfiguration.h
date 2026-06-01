// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "MidiDeclaredStreamConfiguration.g.h"

namespace winrt::Windows::Devices::Midi2::Enumeration::implementation
{
    struct MidiDeclaredStreamConfiguration : MidiDeclaredStreamConfigurationT<MidiDeclaredStreamConfiguration>
    {
        MidiDeclaredStreamConfiguration() = default;
        MidiDeclaredStreamConfiguration(
            _In_ midi2enum::MidiProtocol const protocol,
            _In_ bool const receiveJitterReductionTimestamps,
            _In_ bool const sendJitterReductionTimestamps
        )
        {
            m_protocol = protocol;
            m_receiveJitterReductionTimestamps = receiveJitterReductionTimestamps;
            m_sendJitterReductionTimestamps = sendJitterReductionTimestamps;
        }

        bool IsReadOnly() const noexcept { return m_isReadOnly; }

        midi2enum::MidiProtocol Protocol() const noexcept { return m_protocol; }
        void Protocol(_In_ midi2enum::MidiProtocol const& value)
        {
            if (IsReadOnly()) return;

            m_protocol = value;
        }

        bool ReceiveJitterReductionTimestamps() const noexcept { return m_receiveJitterReductionTimestamps; }
        void ReceiveJitterReductionTimestamps(_In_ bool const value)
        {
            if (IsReadOnly()) return;

            m_receiveJitterReductionTimestamps = value;
        }

        bool SendJitterReductionTimestamps() const noexcept { return m_sendJitterReductionTimestamps; }
        void SendJitterReductionTimestamps(_In_ bool const value)
        {
            if (IsReadOnly()) return;

            m_sendJitterReductionTimestamps = value;
        }

        void InternalSetReadOnly() noexcept { m_isReadOnly = true; }


    private:
        bool m_isReadOnly{ false };

        midi2enum::MidiProtocol m_protocol{};
        bool m_receiveJitterReductionTimestamps{ false };
        bool m_sendJitterReductionTimestamps{ false };

    };
}
namespace winrt::Windows::Devices::Midi2::Enumeration::factory_implementation
{
    struct MidiDeclaredStreamConfiguration : MidiDeclaredStreamConfigurationT<MidiDeclaredStreamConfiguration, implementation::MidiDeclaredStreamConfiguration>
    {};
}
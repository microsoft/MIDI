// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Diagnostics.MidiServicePingResponse.g.h"

namespace winrt::Windows::Devices::Midi2::Diagnostics::implementation
{
    struct MidiServicePingResponse : MidiServicePingResponseT<MidiServicePingResponse>
    {
        MidiServicePingResponse() = default;

        uint32_t SourceId() const noexcept { return m_sourceId; }
        uint32_t Index() const noexcept { return m_index; }
        internal::MidiTimestamp ClientSendMidiTimestamp() const noexcept { return m_clientSendMidiTimestamp; }
        internal::MidiTimestamp ServiceReportedMidiTimestamp() const noexcept { return m_serviceReportedMidiTimestamp; }
        internal::MidiTimestamp ClientReceiveMidiTimestamp() const noexcept { return m_clientReceiveMidiTimestamp; }
        internal::MidiTimestamp ClientDeltaTimestamp() const noexcept { return m_clientDeltaTimestamp; }

        void InternalInitialize(
            _In_ uint32_t const sourceId, 
            _In_ uint32_t const index,
            _In_ internal::MidiTimestamp const clientSendMidiTimestamp
            )
        {
            m_sourceId = sourceId;
            m_index = index;
            m_clientSendMidiTimestamp = clientSendMidiTimestamp;
        };

        void InternalSetResponseData(
            _In_ internal::MidiTimestamp const serviceReportedMidiTimestamp,
            _In_ internal::MidiTimestamp const clientReceiveMidiTimestamp)
        {
            m_serviceReportedMidiTimestamp = serviceReportedMidiTimestamp;
            m_clientReceiveMidiTimestamp = clientReceiveMidiTimestamp;

            m_clientDeltaTimestamp = m_clientReceiveMidiTimestamp - m_clientSendMidiTimestamp;
        }

    private:
        uint32_t m_sourceId{ 0 };
        uint32_t m_index{ 0 };
        internal::MidiTimestamp m_clientSendMidiTimestamp{ 0 };
        internal::MidiTimestamp m_serviceReportedMidiTimestamp{ 0 };
        internal::MidiTimestamp m_clientReceiveMidiTimestamp{ 0 };
        internal::MidiTimestamp m_clientDeltaTimestamp{ 0 };


    };
}
